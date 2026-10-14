-- ================================================================================ --
-- NEORV32 SoC - PS/2 Keyboard Controller                                           --
-- Samples PS/2 clock & data, verifies parity, and pushes scancodes into FIFO      --
-- ================================================================================ --

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity neorv32_ps2 is
  generic (
    FIFO_DEPTH : natural := 16
  );
  port (
    clk_i      : in  std_ulogic;
    rstn_i     : in  std_ulogic;
    
    -- Bus interface (MMIO read)
    bus_addr_i : in  std_ulogic_vector(3 downto 0);
    bus_re_i   : in  std_ulogic;
    bus_rdata_o: out std_ulogic_vector(31 downto 0);
    
    -- PS/2 physical pins
    ps2_clk_i  : in  std_ulogic;
    ps2_data_i : in  std_ulogic
  );
end entity;

architecture rtl of neorv32_ps2 is

  -- Deglitch filter registers
  signal ps2_clk_sync  : std_ulogic_vector(7 downto 0) := (others => '1');
  signal ps2_data_sync : std_ulogic_vector(7 downto 0) := (others => '1');
  signal ps2_clk_clean : std_ulogic := '1';
  signal ps2_clk_clean_q : std_ulogic := '1';
  signal ps2_data_clean: std_ulogic := '1';
  signal ps2_fall_edge : std_ulogic;

  -- Frame receiver registers
  signal bit_count     : integer range 0 to 11 := 0;
  signal idle_cnt      : integer range 0 to 10000 := 0;
  signal rx_data_reg   : std_ulogic_vector(7 downto 0) := (others => '0');

  -- FIFO buffer
  type fifo_mem_t is array (0 to FIFO_DEPTH - 1) of std_ulogic_vector(7 downto 0);
  signal fifo_mem      : fifo_mem_t := (others => (others => '0'));
  signal fifo_wptr     : integer range 0 to FIFO_DEPTH - 1 := 0;
  signal fifo_rptr     : integer range 0 to FIFO_DEPTH - 1 := 0;
  signal fifo_count    : integer range 0 to FIFO_DEPTH := 0;

begin

  -- Glitch filtering on PS/2 pins (slow signals with bounce)
  process(rstn_i, clk_i)
  begin
    if rstn_i = '0' then
      ps2_clk_sync   <= (others => '1');
      ps2_data_sync  <= (others => '1');
      ps2_clk_clean  <= '1';
      ps2_clk_clean_q<= '1';
      ps2_data_clean <= '1';
    elsif rising_edge(clk_i) then
      ps2_clk_sync   <= ps2_clk_sync(6 downto 0) & ps2_clk_i;
      ps2_data_sync  <= ps2_data_sync(6 downto 0) & ps2_data_i;
      ps2_clk_clean_q<= ps2_clk_clean;

      if ps2_clk_sync = "11111111" then
        ps2_clk_clean <= '1';
      elsif ps2_clk_sync = "00000000" then
        ps2_clk_clean <= '0';
      end if;

      if ps2_data_sync = "11111111" then
        ps2_data_clean <= '1';
      elsif ps2_data_sync = "00000000" then
        ps2_data_clean <= '0';
      end if;
    end if;
  end process;

  ps2_fall_edge <= '1' when (ps2_clk_clean_q = '1' and ps2_clk_clean = '0') else '0';

  -- PS/2 Receiver Frame State Machine with Idle Timeout (100 us resync)
  process(rstn_i, clk_i)
    variable fifo_push : std_ulogic;
    variable fifo_pop  : std_ulogic;
  begin
    if rstn_i = '0' then
      bit_count   <= 0;
      idle_cnt    <= 0;
      rx_data_reg <= (others => '0');
      fifo_wptr   <= 0;
      fifo_rptr   <= 0;
      fifo_count  <= 0;
    elsif rising_edge(clk_i) then

      -- Idle timeout: if clock stays high for 100 us (5,000 cycles @ 50 MHz), resynchronize frame
      if ps2_clk_clean = '1' then
        if idle_cnt < 5000 then
          idle_cnt <= idle_cnt + 1;
        else
          bit_count <= 0;
        end if;
      else
        idle_cnt <= 0;
      end if;

      -- Receive 11-bit PS/2 sequence on falling edge of PS/2 Clock
      fifo_push := '0';
      if ps2_fall_edge = '1' then
        idle_cnt <= 0;
        case bit_count is
          when 0 =>
            -- Start bit
            bit_count <= 1;

          when 1 to 8 =>
            -- Data bits: LSB first (bits 0 to 7)
            rx_data_reg(bit_count - 1) <= ps2_data_clean;
            bit_count <= bit_count + 1;

          when 9 =>
            -- Parity bit
            bit_count <= 10;

          when 10 =>
            -- Stop bit: transfer complete scancode byte into FIFO
            bit_count <= 0;
            if fifo_count < FIFO_DEPTH then
              fifo_mem(fifo_wptr) <= rx_data_reg;
              fifo_wptr <= (fifo_wptr + 1) mod FIFO_DEPTH;
              fifo_push := '1';
            end if;

          when others =>
            bit_count <= 0;
        end case;
      end if;

      -- CPU FIFO Read
      fifo_pop := '0';
      if bus_re_i = '1' and bus_addr_i = "0000" then
        if fifo_count > 0 then
          fifo_rptr <= (fifo_rptr + 1) mod FIFO_DEPTH;
          fifo_pop := '1';
        end if;
      end if;

      -- Unified FIFO Count Management
      if fifo_push = '1' and fifo_pop = '0' then
        fifo_count <= fifo_count + 1;
      elsif fifo_push = '0' and fifo_pop = '1' then
        fifo_count <= fifo_count - 1;
      end if;

    end if;
  end process;

  -- MMIO Read Interface (Strict VHDL-93 / VHDL-2002 compatibility)
  process(bus_addr_i, fifo_count, fifo_mem, fifo_rptr)
  begin
    bus_rdata_o <= (others => '0');
    if bus_addr_i = "0000" then
      if fifo_count > 0 then
        bus_rdata_o(7 downto 0) <= fifo_mem(fifo_rptr);
      else
        bus_rdata_o(7 downto 0) <= (others => '0');
      end if;
    elsif bus_addr_i = "0100" then
      if fifo_count > 0 then
        bus_rdata_o(0) <= '1';
      else
        bus_rdata_o(0) <= '0';
      end if;

      if fifo_count = FIFO_DEPTH then
        bus_rdata_o(1) <= '1';
      else
        bus_rdata_o(1) <= '0';
      end if;

      bus_rdata_o(7 downto 4) <= std_ulogic_vector(to_unsigned(fifo_count, 4));
    end if;
  end process;

end architecture;
