library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- ============================================================
-- Entity: neorv32_memory_crossbar
-- Description:
--   Memory crossbar connecting a 32-bit CPU master and a
--   256-bit GEMM Soft DMA master to a shared 256-bit memory.
--
-- Features:
--   1. Fixed-priority arbitration (DMA has higher priority).
--   2. CPU 32-bit to memory 256-bit data width conversion.
--   3. Byte-enable and write-data lane selection for CPU access.
--   4. Direct 256-bit DMA-to-memory routing.
--   5. Acknowledgment and read-data response routing.
--
-- Note:
--   Despite the FCFS label in the original implementation,
--   the arbitration policy is fixed priority, not true FCFS.
-- ============================================================

entity neorv32_memory_crossbar is
  port (
    clk_i   : in  std_ulogic;
    rstn_i  : in  std_ulogic;

    -- --------------------------------------------------------
    -- Master 1: CPU interface (32-bit)
    -- req_addr  : Byte address of the requested access
    -- req_wdata : 32-bit write data
    -- req_be    : 4-bit byte-enable mask
    -- req_rw    : Read/write control
    -- req_stb   : Request strobe
    -- rsp_rdata : 32-bit read response
    -- rsp_ack   : Memory transaction acknowledgment
    -- --------------------------------------------------------
    cpu_req_addr  : in  std_ulogic_vector(31 downto 0);
    cpu_req_wdata : in  std_ulogic_vector(31 downto 0);
    cpu_req_be    : in  std_ulogic_vector(3 downto 0);
    cpu_req_rw    : in  std_ulogic;
    cpu_req_stb   : in  std_ulogic;
    cpu_rsp_rdata : out std_ulogic_vector(31 downto 0);
    cpu_rsp_ack   : out std_ulogic;

    -- --------------------------------------------------------
    -- Master 2: GEMM Soft DMA interface (256-bit)
    -- Supports wide data transfers for matrix operations.
    -- --------------------------------------------------------
    dma_req_addr  : in  std_ulogic_vector(31 downto 0);
    dma_req_wdata : in  std_ulogic_vector(255 downto 0);
    dma_req_be    : in  std_ulogic_vector(31 downto 0);
    dma_req_rw    : in  std_ulogic;
    dma_req_stb   : in  std_ulogic;
    dma_rsp_rdata : out std_ulogic_vector(255 downto 0);
    dma_rsp_ack   : out std_ulogic;

    -- --------------------------------------------------------
    -- Slave: Shared memory interface (256-bit)
    -- One memory transaction transfers up to 32 bytes.
    -- --------------------------------------------------------
    mem_req_addr  : out std_ulogic_vector(31 downto 0);
    mem_req_wdata : out std_ulogic_vector(255 downto 0);
    mem_req_be    : out std_ulogic_vector(31 downto 0);
    mem_req_rw    : out std_ulogic;
    mem_req_stb   : out std_ulogic;
    mem_rsp_rdata : in  std_ulogic_vector(255 downto 0);
    mem_rsp_ack   : in  std_ulogic
  );
end entity;

architecture rtl of neorv32_memory_crossbar is

  -- ----------------------------------------------------------
  -- Arbitration state encoding
  -- ARB_IDLE : No master currently owns the memory interface.
  -- ARB_CPU  : CPU owns the memory interface.
  -- ARB_DMA  : DMA owns the memory interface.
  -- ----------------------------------------------------------
  type arb_state_t is (ARB_IDLE, ARB_CPU, ARB_DMA);
  signal arb_state : arb_state_t;

  -- Stores the selected 32-bit word position within a
  -- 256-bit memory word for CPU read-data extraction.
  -- Eight 32-bit words fit into one 256-bit memory word.
  signal cpu_word_sel : integer range 0 to 7;

begin

  -- ==========================================================
  -- Process 1: Arbitration State Machine
  -- ==========================================================
  -- Synchronous state transitions controlled by clk_i.
  -- Active-low reset returns the controller to ARB_IDLE.
  --
  -- Arbitration priority:
  --   1. DMA request
  --   2. CPU request
  --
  -- The selected master retains ownership until its request
  -- strobe is deasserted.
  -- ==========================================================
  process(clk_i)
  begin
    if rising_edge(clk_i) then
      if rstn_i = '0' then
        arb_state <= ARB_IDLE;
        cpu_word_sel <= 0;
      else
        case arb_state is

          when ARB_IDLE =>
            -- Give DMA priority when both masters request access.
            if dma_req_stb = '1' then
              arb_state <= ARB_DMA;

            elsif cpu_req_stb = '1' then
              arb_state <= ARB_CPU;

              -- Capture the 32-bit word offset within the
              -- 256-bit memory word (address bits 4 downto 2).
              cpu_word_sel <=
                to_integer(unsigned(cpu_req_addr(4 downto 2)));
            end if;

          when ARB_CPU =>
            -- Release the memory interface when CPU drops strobe.
            if cpu_req_stb = '0' then
              arb_state <= ARB_IDLE;
            end if;

          when ARB_DMA =>
            -- Release the memory interface when DMA drops strobe.
            if dma_req_stb = '0' then
              arb_state <= ARB_IDLE;
            end if;

        end case;
      end if;
    end if;
  end process;

  -- ==========================================================
  -- Process 2: Combinational Request Routing and Response Mux
  -- ==========================================================
  -- Routes the selected master's request to the shared memory.
  --
  -- CPU path:
  --   32-bit address/data/byte-enable
  --       -> 256-bit memory interface
  --
  -- DMA path:
  --   256-bit request passes directly to memory.
  --
  -- Responses are routed back to the active master.
  -- ==========================================================
  process(arb_state,
          cpu_req_addr, cpu_req_wdata, cpu_req_be,
          cpu_req_rw, cpu_req_stb,
          dma_req_addr, dma_req_wdata, dma_req_be,
          dma_req_rw, dma_req_stb,
          mem_rsp_ack, mem_rsp_rdata, cpu_word_sel)

    -- Expanded CPU byte-enable mask for the 256-bit memory.
    variable v_cpu_be_256 : std_ulogic_vector(31 downto 0);

    -- Expanded CPU write data, placed in the selected 32-bit lane.
    variable v_cpu_wdata_256 : std_ulogic_vector(255 downto 0);

    -- Current CPU word index within a 256-bit memory word.
    variable v_word_idx : integer range 0 to 7;

  begin
    -- --------------------------------------------------------
    -- Default assignments prevent unintended latch inference.
    -- --------------------------------------------------------
    mem_req_addr  <= (others => '0');
    mem_req_wdata <= (others => '0');
    mem_req_be    <= (others => '0');
    mem_req_rw    <= '0';
    mem_req_stb   <= '0';

    cpu_rsp_rdata <= (others => '0');
    cpu_rsp_ack   <= '0';

    dma_rsp_rdata <= (others => '0');
    dma_rsp_ack   <= '0';

    -- Determine which 32-bit lane is selected by the CPU address.
    v_word_idx :=
      to_integer(unsigned(cpu_req_addr(4 downto 2)));

    -- Initialize expanded CPU signals.
    v_cpu_be_256   := (others => '0');
    v_cpu_wdata_256 := (others => '0');

    -- --------------------------------------------------------
    -- Case 1: CPU owns the memory interface
    -- --------------------------------------------------------
    if arb_state = ARB_CPU then

      -- Align the memory address to a 32-byte boundary.
      mem_req_addr <= cpu_req_addr(31 downto 5) & "00000";

      -- Place the CPU byte-enable bits into the selected lane.
      v_cpu_be_256(v_word_idx*4 + 3 downto v_word_idx*4)
        := cpu_req_be;

      -- Place the 32-bit CPU write data into the selected lane.
      v_cpu_wdata_256(v_word_idx*32 + 31 downto v_word_idx*32)
        := cpu_req_wdata;

      -- Drive the expanded request to the memory.
      mem_req_wdata <= v_cpu_wdata_256;
      mem_req_be    <= v_cpu_be_256;
      mem_req_rw    <= cpu_req_rw;
      mem_req_stb   <= cpu_req_stb;

      -- Return the memory acknowledgment to the CPU.
      cpu_rsp_ack <= mem_rsp_ack;

      -- Extract the selected 32-bit word from the 256-bit
      -- memory read response.
      cpu_rsp_rdata <=
        mem_rsp_rdata(cpu_word_sel*32 + 31 downto cpu_word_sel*32);

    -- --------------------------------------------------------
    -- Case 2: DMA owns the memory interface
    -- --------------------------------------------------------
    elsif arb_state = ARB_DMA then

      -- DMA already uses the memory's native 256-bit width.
      -- Therefore, address, write data, byte enables and
      -- read/write control are passed through directly.
      mem_req_addr  <= dma_req_addr;
      mem_req_wdata <= dma_req_wdata;
      mem_req_be    <= dma_req_be;
      mem_req_rw    <= dma_req_rw;
      mem_req_stb   <= dma_req_stb;

      -- Route the memory response back to the DMA master.
      dma_rsp_ack   <= mem_rsp_ack;
      dma_rsp_rdata <= mem_rsp_rdata;

    -- --------------------------------------------------------
    -- Case 3: Idle state with combinational request forwarding
    -- --------------------------------------------------------
    else

      -- Forward a DMA request immediately if present.
      -- This does not itself grant registered ownership.
      if dma_req_stb = '1' then

        mem_req_addr  <= dma_req_addr;
        mem_req_wdata <= dma_req_wdata;
        mem_req_be    <= dma_req_be;
        mem_req_rw    <= dma_req_rw;
        mem_req_stb   <= '1';

      -- Otherwise, forward a CPU request after width conversion.
      elsif cpu_req_stb = '1' then

        -- Align the CPU address to a 256-bit memory boundary.
        mem_req_addr <= cpu_req_addr(31 downto 5) & "00000";

        -- Expand the CPU byte enables and write data.
        v_cpu_be_256(v_word_idx*4 + 3 downto v_word_idx*4)
          := cpu_req_be;

        v_cpu_wdata_256(v_word_idx*32 + 31 downto v_word_idx*32)
          := cpu_req_wdata;

        -- Drive the converted CPU request.
        mem_req_wdata <= v_cpu_wdata_256;
        mem_req_be    <= v_cpu_be_256;
        mem_req_rw    <= cpu_req_rw;
        mem_req_stb   <= '1';

        -- Forward memory acknowledgment and read response back to CPU
        cpu_rsp_ack   <= mem_rsp_ack;
        cpu_rsp_rdata <= mem_rsp_rdata(v_word_idx*32 + 31 downto v_word_idx*32);
      end if;
    end if;
  end process;

end architecture;
