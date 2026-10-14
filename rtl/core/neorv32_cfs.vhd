-- ================================================================================ --
-- NEORV32 SoC - Custom Functions Subsystem (CFS) for GEMM Accelerator              --
-- Featuring: 2D Strided DMA Engine, Bounds Checking, & Pipelined MAC Array         --
-- ================================================================================ --

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library neorv32;
use neorv32.neorv32_package.all;

entity neorv32_cfs is
  port (
    clk_i      : in  std_ulogic;
    rstn_i     : in  std_ulogic;
    
    req_addr_i : in  std_ulogic_vector(15 downto 0);
    req_data_i : in  std_ulogic_vector(31 downto 0);
    req_ben_i  : in  std_ulogic_vector(3 downto 0);
    req_stb_i  : in  std_ulogic;
    req_rw_i   : in  std_ulogic;
    rsp_data_o : out std_ulogic_vector(31 downto 0);
    rsp_ack_o  : out std_ulogic;
    irq_o      : out std_ulogic;
    
    cfs_in_i   : in  std_ulogic_vector(255 downto 0);
    cfs_out_o  : out std_ulogic_vector(255 downto 0);
    cfs_req_o  : out bus_req_t;
    cfs_rsp_i  : in  bus_rsp_t;
    
    dma_req_addr  : out std_ulogic_vector(31 downto 0);
    dma_req_wdata : out std_ulogic_vector(255 downto 0);
    dma_req_be    : out std_ulogic_vector(31 downto 0);
    dma_req_rw    : out std_ulogic;
    dma_req_stb   : out std_ulogic;
    dma_rsp_rdata : in  std_ulogic_vector(255 downto 0);
    dma_rsp_ack   : in  std_ulogic
  );
end entity;

architecture rtl of neorv32_cfs is

    signal reg_base_a    : unsigned(31 downto 0);
    signal reg_base_b    : unsigned(31 downto 0);
    signal reg_base_c    : unsigned(31 downto 0);
    signal reg_stride_a  : unsigned(15 downto 0);
    signal reg_stride_b  : unsigned(15 downto 0);
    signal reg_stride_c  : unsigned(15 downto 0);
    signal reg_bounds    : unsigned(31 downto 0);
    
    signal cmd_start     : std_ulogic;
    signal cmd_clear     : std_ulogic;
    signal cmd_store     : std_ulogic;
    signal status_done   : std_ulogic;

    type state_t is (S_IDLE, 
                     S_FETCH_A, S_FETCH_A_WAIT, 
                     S_FETCH_B, S_FETCH_B_WAIT, 
                     S_COMP_INIT, S_COMP_ROW_INIT, S_COMP_MAC, S_COMP_ACCUM, 
                     S_STORE_C, S_STORE_C_WAIT, S_DONE);
    signal state : state_t;

    signal row_cnt : integer range 0 to 7;
    signal col_cnt : integer range 0 to 7;
    signal k_cnt   : integer range 0 to 7;

    type buffer_8x8_8b_t is array (0 to 63) of signed(7 downto 0);
    signal buf_A : buffer_8x8_8b_t;
    signal buf_B : buffer_8x8_8b_t;
    
    type buffer_8x8_32b_t is array (0 to 63) of signed(31 downto 0);
    signal buf_C : buffer_8x8_32b_t;
    
    type sum_reg_t is array (0 to 7) of signed(31 downto 0);
    signal sum_reg : sum_reg_t;

    -- Hardware performance counters (accumulated across all tiles, reset on cmd_clear)
    signal perf_fetch_a_cycles  : unsigned(31 downto 0); -- cycles in S_FETCH_A + S_FETCH_A_WAIT
    signal perf_fetch_b_cycles  : unsigned(31 downto 0); -- cycles in S_FETCH_B + S_FETCH_B_WAIT
    signal perf_compute_cycles  : unsigned(31 downto 0); -- cycles in S_COMP_* states
    signal perf_store_cycles    : unsigned(31 downto 0); -- cycles in S_STORE_C + S_STORE_C_WAIT
    signal perf_rst             : std_ulogic;

begin

    cfs_out_o <= (others => '0');
    cfs_req_o <= req_terminate_c;
    irq_o     <= status_done;

    process(rstn_i, clk_i)
    begin
        if rstn_i = '0' then
            reg_base_a <= (others => '0');
            reg_base_b <= (others => '0');
            reg_base_c <= (others => '0');
            reg_stride_a <= (others => '0');
            reg_stride_b <= (others => '0');
            reg_stride_c <= (others => '0');
            reg_bounds <= (others => '0');
            cmd_start <= '0';
            cmd_clear <= '0';
            cmd_store <= '0';
            perf_rst  <= '0';
            rsp_ack_o <= '0';
            rsp_data_o <= (others => '0');
        elsif rising_edge(clk_i) then
            rsp_ack_o <= req_stb_i;
            rsp_data_o <= (others => '0');
            perf_rst  <= '0';
            
            if req_stb_i = '1' and req_rw_i = '1' then
                case req_addr_i(4 downto 2) is
                    when "000" => reg_base_a <= unsigned(req_data_i);
                    when "001" => reg_base_b <= unsigned(req_data_i);
                    when "010" => reg_base_c <= unsigned(req_data_i);
                    when "011" => reg_stride_a <= unsigned(req_data_i(15 downto 0));
                    when "100" => reg_stride_b <= unsigned(req_data_i(15 downto 0));
                    when "101" => reg_stride_c <= unsigned(req_data_i(15 downto 0));
                    when "110" => 
                        cmd_start <= req_data_i(0);
                        cmd_clear <= req_data_i(1);
                        cmd_store <= req_data_i(2);
                        -- Writing bit 3 (=8) resets the performance counters
                        if req_data_i(3) = '1' then
                            perf_rst <= '1';
                        end if;
                    when "111" => reg_bounds <= unsigned(req_data_i);
                    when others => null;
                end case;
            end if;
            
            if req_stb_i = '1' and req_rw_i = '0' then
                case req_addr_i(4 downto 2) is
                    when "110" => rsp_data_o(2) <= status_done;
                    -- Performance counter readout (read-only)
                    when "000" => rsp_data_o <= std_ulogic_vector(perf_fetch_a_cycles);
                    when "001" => rsp_data_o <= std_ulogic_vector(perf_fetch_b_cycles);
                    when "010" => rsp_data_o <= std_ulogic_vector(perf_compute_cycles);
                    when "011" => rsp_data_o <= std_ulogic_vector(perf_store_cycles);
                    when others => null;
                end case;
            end if;
        end if;
    end process;

    process(rstn_i, clk_i)
        variable addr_val : unsigned(31 downto 0);
        variable offset   : integer range 0 to 31;
        variable tile_M   : integer range 0 to 255;
        variable tile_K   : integer range 0 to 255;
        variable tile_N   : integer range 0 to 255;
    begin
        if rstn_i = '0' then
            state <= S_IDLE;
            dma_req_stb <= '0';
            status_done <= '0';
            perf_fetch_a_cycles <= (others => '0');
            perf_fetch_b_cycles <= (others => '0');
            perf_compute_cycles <= (others => '0');
            perf_store_cycles   <= (others => '0');
            for i in 0 to 63 loop
                buf_C(i) <= (others => '0');
            end loop;
        elsif rising_edge(clk_i) then
            dma_req_stb <= '0';
            tile_M := to_integer(reg_bounds(23 downto 16));
            tile_K := to_integer(reg_bounds(15 downto 8));
            tile_N := to_integer(reg_bounds(7 downto 0));
            
            -- Reset performance counters on software command
            if perf_rst = '1' then
                perf_fetch_a_cycles <= (others => '0');
                perf_fetch_b_cycles <= (others => '0');
                perf_compute_cycles <= (others => '0');
                perf_store_cycles   <= (others => '0');
            else
                -- Per-cycle performance counter increments
                case state is
                    when S_FETCH_A | S_FETCH_A_WAIT =>
                        perf_fetch_a_cycles <= perf_fetch_a_cycles + 1;
                    when S_FETCH_B | S_FETCH_B_WAIT =>
                        perf_fetch_b_cycles <= perf_fetch_b_cycles + 1;
                    when S_COMP_INIT | S_COMP_ROW_INIT | S_COMP_MAC | S_COMP_ACCUM =>
                        perf_compute_cycles <= perf_compute_cycles + 1;
                    when S_STORE_C | S_STORE_C_WAIT =>
                        perf_store_cycles <= perf_store_cycles + 1;
                    when others => null;
                end case;
            end if;
            
            case state is
                when S_IDLE =>
                    if cmd_start = '1' then
                        if cmd_clear = '1' then
                            for i in 0 to 63 loop
                                buf_C(i) <= (others => '0');
                            end loop;
                        end if;
                        row_cnt <= 0;
                        col_cnt <= 0;
                        state <= S_FETCH_A;
                        status_done <= '0';
                    end if;
                    
                when S_FETCH_A =>
                    if row_cnt < tile_M then
                        addr_val := reg_base_a + resize(to_unsigned(row_cnt, 16) * reg_stride_a, 32) + to_unsigned(col_cnt, 32);
                        dma_req_addr <= std_ulogic_vector(addr_val);
                        dma_req_rw <= '0';
                        dma_req_be <= (others => '1');
                        dma_req_stb <= '1';
                        state <= S_FETCH_A_WAIT;
                    else
                        for c in 0 to 7 loop
                            buf_A(row_cnt*8 + c) <= (others => '0');
                        end loop;
                        if row_cnt = 7 then
                            row_cnt <= 0;
                            col_cnt <= 0;
                            state <= S_FETCH_B;
                        else
                            row_cnt <= row_cnt + 1;
                            state <= S_FETCH_A;
                        end if;
                    end if;
                    
                when S_FETCH_A_WAIT =>
                    if dma_rsp_ack = '1' then
                        addr_val := reg_base_a + resize(to_unsigned(row_cnt, 16) * reg_stride_a, 32) + to_unsigned(col_cnt, 32);
                        offset := to_integer(addr_val(4 downto 0));
                        for c in 0 to 7 loop
                            if (offset + c) <= 31 then
                                buf_A(row_cnt*8 + c) <= signed(dma_rsp_rdata((offset+c)*8+7 downto (offset+c)*8));
                            else
                                buf_A(row_cnt*8 + c) <= (others => '0');
                            end if;
                        end loop;
                        
                        if row_cnt = 7 then
                            row_cnt <= 0;
                            col_cnt <= 0;
                            state <= S_FETCH_B;
                        else
                            row_cnt <= row_cnt + 1;
                            state <= S_FETCH_A;
                        end if;
                    else
                        dma_req_stb <= '1';
                    end if;

                when S_FETCH_B =>
                    if row_cnt < tile_K then
                        addr_val := reg_base_b + resize(to_unsigned(row_cnt, 16) * reg_stride_b, 32) + to_unsigned(col_cnt, 32);
                        dma_req_addr <= std_ulogic_vector(addr_val);
                        dma_req_rw <= '0';
                        dma_req_be <= (others => '1');
                        dma_req_stb <= '1';
                        state <= S_FETCH_B_WAIT;
                    else
                        for c in 0 to 7 loop
                            buf_B(row_cnt*8 + c) <= (others => '0');
                        end loop;
                        if row_cnt = 7 then
                            col_cnt <= 0;
                            row_cnt <= 0;
                            state <= S_COMP_INIT;
                        else
                            row_cnt <= row_cnt + 1;
                            state <= S_FETCH_B;
                        end if;
                    end if;
                    
                when S_FETCH_B_WAIT =>
                    if dma_rsp_ack = '1' then
                        addr_val := reg_base_b + resize(to_unsigned(row_cnt, 16) * reg_stride_b, 32) + to_unsigned(col_cnt, 32);
                        offset := to_integer(addr_val(4 downto 0));
                        for c in 0 to 7 loop
                            if (offset + c) <= 31 then
                                buf_B(row_cnt*8 + c) <= signed(dma_rsp_rdata((offset+c)*8+7 downto (offset+c)*8));
                            else
                                buf_B(row_cnt*8 + c) <= (others => '0');
                            end if;
                        end loop;
                        
                        if row_cnt = 7 then
                            col_cnt <= 0;
                            row_cnt <= 0;
                            state <= S_COMP_INIT;
                        else
                            row_cnt <= row_cnt + 1;
                            state <= S_FETCH_B;
                        end if;
                    else
                        dma_req_stb <= '1';
                    end if;

                when S_COMP_INIT =>
                    row_cnt <= 0;
                    state <= S_COMP_ROW_INIT;
                    
                when S_COMP_ROW_INIT =>
                    for j in 0 to 7 loop
                        sum_reg(j) <= (others => '0');
                    end loop;
                    k_cnt <= 0;
                    state <= S_COMP_MAC;
                    
                when S_COMP_MAC =>
                    for j in 0 to 7 loop
                        sum_reg(j) <= sum_reg(j) + resize(buf_A(row_cnt*8 + k_cnt) * buf_B(k_cnt*8 + j), 32);
                    end loop;
                    if k_cnt = 7 then
                        state <= S_COMP_ACCUM;
                    else
                        k_cnt <= k_cnt + 1;
                    end if;
                    
                when S_COMP_ACCUM =>
                    for j in 0 to 7 loop
                        buf_C(row_cnt*8 + j) <= buf_C(row_cnt*8 + j) + sum_reg(j);
                    end loop;
                    if row_cnt = 7 then
                        if cmd_store = '1' then
                            row_cnt <= 0;
                            col_cnt <= 0;
                            state <= S_STORE_C;
                        else
                            state <= S_DONE;
                        end if;
                    else
                        row_cnt <= row_cnt + 1;
                        state <= S_COMP_ROW_INIT;
                    end if;

                when S_STORE_C =>
                    if row_cnt < tile_M then
                        addr_val := reg_base_c + shift_left(resize(to_unsigned(row_cnt, 16) * reg_stride_c + to_unsigned(col_cnt, 16), 32), 2);
                        dma_req_addr <= std_ulogic_vector(addr_val);
                        dma_req_rw <= '1';
                        
                        dma_req_wdata <= (others => '0');
                        dma_req_be <= (others => '0');
                        
                        offset := to_integer(addr_val(4 downto 2));
                        for c in 0 to 7 loop
                            if (offset + c) <= 7 then
                                dma_req_wdata((offset+c)*32+31 downto (offset+c)*32) <= std_ulogic_vector(buf_C(row_cnt*8 + c));
                                dma_req_be((offset+c)*4+3 downto (offset+c)*4) <= "1111";
                            end if;
                        end loop;
                        
                        dma_req_stb <= '1';
                        state <= S_STORE_C_WAIT;
                    else
                        if row_cnt = 7 then
                            col_cnt <= 0;
                            row_cnt <= 0;
                            state <= S_DONE;
                        else
                            row_cnt <= row_cnt + 1;
                            state <= S_STORE_C;
                        end if;
                    end if;
                    
                when S_STORE_C_WAIT =>
                    if dma_rsp_ack = '1' then
                        if row_cnt = 7 then
                            col_cnt <= 0;
                            row_cnt <= 0;
                            state <= S_DONE;
                        else
                            row_cnt <= row_cnt + 1;
                            state <= S_STORE_C;
                        end if;
                    else
                        dma_req_stb <= '1';
                    end if;

                when S_DONE =>
                    status_done <= '1';
                    if cmd_start = '0' then
                        status_done <= '0';
                        state <= S_IDLE;
                    end if;
                    
                when others =>
                    state <= S_IDLE;
            end case;
        end if;
    end process;

end architecture;
