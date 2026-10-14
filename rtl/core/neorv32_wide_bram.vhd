-- ================================================================================ --
-- GEMMrv32 - Wide Block RAM (256-bit)                                              --
-- ================================================================================ --
-- Synthesisable 128KB Block RAM with a single 256-bit Wishbone slave interface.    --
-- Designed to attach to the Asymmetric Memory Crossbar.                            --
-- ================================================================================ --

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity neorv32_wide_bram is
  generic (
    MEM_SIZE_BYTES : natural := 131072 -- 128 KB
  );
  port (
    clk_i   : in  std_ulogic;
    rstn_i  : in  std_ulogic;

    -- 256-bit Wishbone Slave Port
    req_addr  : in  std_ulogic_vector(31 downto 0);
    req_wdata : in  std_ulogic_vector(255 downto 0);
    req_be    : in  std_ulogic_vector(31 downto 0);
    req_rw    : in  std_ulogic;
    req_stb   : in  std_ulogic;
    rsp_rdata : out std_ulogic_vector(255 downto 0);
    rsp_ack   : out std_ulogic
  );
end entity;

architecture rtl of neorv32_wide_bram is

  -- Number of 256-bit (32-byte) words
  constant NUM_WORDS : natural := MEM_SIZE_BYTES / 32;
  
  -- Memory array: 256 bits per row
  type mem_t is array (0 to NUM_WORDS-1) of std_ulogic_vector(255 downto 0);
  
  signal mem : mem_t;
  
  -- Prevent synthesis tools from mapping this to distributed RAM
  attribute ram_style : string;
  attribute ram_style of mem : signal is "block";
  
begin

  process(clk_i)
    variable v_word_addr : integer;
  begin
    if rising_edge(clk_i) then
      -- Default: drop ACK
      rsp_ack <= '0';
      
      if req_stb = '1' then
        -- Convert byte address to 32-byte word index (addr / 32).
        -- Mask to 17 bits (128KB) to ignore the 0x80000000 base address offset.
        v_word_addr := to_integer(unsigned(req_addr(16 downto 5)));
        
        -- Address bounds check (prevent out-of-bounds simulation crash/synthesis warnings)
        if v_word_addr < NUM_WORDS then
          if req_rw = '1' then
            -- Write access with byte enables
            for i in 0 to 31 loop
              if req_be(i) = '1' then
                mem(v_word_addr)(i*8+7 downto i*8) <= req_wdata(i*8+7 downto i*8);
              end if;
            end loop;
            -- BRAMs usually have 1 cycle write latency
            rsp_ack <= '1';
          else
            -- Read access
            rsp_rdata <= mem(v_word_addr);
            rsp_ack <= '1';
          end if;
        else
          -- Out of bounds access, just ACK to prevent bus lockup
          rsp_ack <= '1';
          rsp_rdata <= (others => '0');
        end if;
      end if;
    end if;
  end process;

end architecture;
