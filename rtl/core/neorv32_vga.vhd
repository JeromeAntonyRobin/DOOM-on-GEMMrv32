-- ================================================================================ --
-- NEORV32 SoC - VGA Framebuffer & Palette Raster Controller                         --
-- Resolution: 160x120 @ 8bpp upscaled 4x to 640x480 @ 60Hz                          --
-- Features:                                                                        --
--   - 19,200 bytes dual-port video RAM (Port A: CPU write, Port B: VGA scanout)    --
--   - 256 x 12-bit RGB Color Lookup Table (Palette CLUT) in distributed RAM        --
--   - Standard 640x480 @ 60Hz timing generator (25 MHz pixel clock derived from 50) --
--   - 12-bit resistor DAC direct output (4-bit R, 4-bit G, 4-bit B, HSYNC, VSYNC)  --
-- ================================================================================ --

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity neorv32_vga is
  port (
    -- SoC Bus Clock & Reset
    clk_i       : in  std_ulogic; -- 50 MHz system clock
    rstn_i      : in  std_ulogic;

    -- MMIO / Wishbone Bus Slave for VRAM & Palette
    -- Addr range:
    --   0x00000 - 0x04B00 : 160x120 Framebuffer (19,200 bytes)
    --   0x10000 - 0x103FF : Palette RAM (256 entries * 4 bytes: [11:8]=R, [7:4]=G, [3:0]=B)
    --   0x10400           : Status / VSync Register ([0] = in_vblank)
    bus_addr_i  : in  std_ulogic_vector(16 downto 0);
    bus_wdata_i : in  std_ulogic_vector(31 downto 0);
    bus_ben_i   : in  std_ulogic_vector(3 downto 0);
    bus_we_i    : in  std_ulogic;
    bus_rdata_o : out std_ulogic_vector(31 downto 0);

    -- VGA Physical Interface (12-bit DAC + Sync)
    vga_red_o   : out std_ulogic_vector(3 downto 0);
    vga_green_o : out std_ulogic_vector(3 downto 0);
    vga_blue_o  : out std_ulogic_vector(3 downto 0);
    vga_hsync_o : out std_ulogic;
    vga_vsync_o : out std_ulogic
  );
end entity;

architecture rtl of neorv32_vga is

  -- 25 MHz pixel clock strobe from 50 MHz clk_i
  signal pclk_div    : std_ulogic := '0';
  signal pclk_tick   : std_ulogic;

  -- 640x480 @ 60 Hz Timing Parameters
  -- H: 640 active + 16 FP + 96 Sync + 48 BP = 800 total
  -- V: 480 active + 10 FP + 2 Sync + 33 BP = 525 total
  signal h_cnt       : integer range 0 to 799 := 0;
  signal v_cnt       : integer range 0 to 524 := 0;

  signal active_area : boolean := false;
  signal hsync_reg   : std_ulogic := '1';
  signal vsync_reg   : std_ulogic := '1';
  signal in_vblank   : std_ulogic := '0';

  -- Framebuffer index calculation:
  -- Pixel coordinates in 160x120 frame:
  --   pixel_x = h_cnt / 4 (0 to 159)
  --   pixel_y = v_cnt / 4 (0 to 119)
  --   linear_addr = pixel_y * 160 + pixel_x
  signal pixel_x     : integer range 0 to 159 := 0;
  signal pixel_y     : integer range 0 to 119 := 0;
  signal vram_raddr  : unsigned(14 downto 0) := (others => '0');

  -- 19,200 bytes Framebuffer (Dual-Port BRAM inference)
  type vram_t is array (0 to 19199) of std_ulogic_vector(7 downto 0);
  signal vram        : vram_t := (others => (others => '0'));
  attribute ram_style : string;
  attribute ram_style of vram : signal is "block";
  signal pixel_idx   : std_ulogic_vector(7 downto 0) := (others => '0');

  -- 256-entry x 12-bit RGB Color Palette (CLUT)
  type palette_t is array (0 to 255) of std_ulogic_vector(11 downto 0);
  signal palette_mem : palette_t := (
    -- Default 16-color EGA/VGA palette fallback
    0 => x"000", 1 => x"00A", 2 => x"0A0", 3 => x"0AA",
    4 => x"A00", 5 => x"A0A", 6 => x"A50", 7 => x"AAA",
    8 => x"555", 9 => x"55F", 10 => x"5F5", 11 => x"5FF",
    12 => x"F55", 13 => x"F5F", 14 => x"FF5", 15 => x"FFF",
    others => x"FFF"
  );
  signal rgb_color   : std_ulogic_vector(11 downto 0);
  signal active_d1   : boolean := false;
  signal active_d2   : boolean := false;

begin

  -- Generate 25 MHz pixel enable tick from 50 MHz system clock
  process(clk_i, rstn_i)
  begin
    if rstn_i = '0' then
      pclk_div <= '0';
    elsif rising_edge(clk_i) then
      pclk_div <= not pclk_div;
    end if;
  end process;
  pclk_tick <= pclk_div;

  -- VGA Sync Generator (640x480 @ 60Hz)
  process(clk_i, rstn_i)
  begin
    if rstn_i = '0' then
      h_cnt       <= 0;
      v_cnt       <= 0;
      hsync_reg   <= '1';
      vsync_reg   <= '1';
      active_area <= false;
      in_vblank   <= '0';
    elsif rising_edge(clk_i) then
      if pclk_tick = '1' then
        -- Horizontal Counter
        if h_cnt = 799 then
          h_cnt <= 0;
          -- Vertical Counter
          if v_cnt = 524 then
            v_cnt <= 0;
          else
            v_cnt <= v_cnt + 1;
          end if;
        else
          h_cnt <= h_cnt + 1;
        end if;

        -- Horizontal Sync Pulse (active LOW: 656 to 751)
        if (h_cnt >= (640 + 16)) and (h_cnt < (640 + 16 + 96)) then
          hsync_reg <= '0';
        else
          hsync_reg <= '1';
        end if;

        -- Vertical Sync Pulse (active LOW: 490 to 491)
        if (v_cnt >= (480 + 10)) and (v_cnt < (480 + 10 + 2)) then
          vsync_reg <= '0';
        else
          vsync_reg <= '1';
        end if;

        -- Active Video Region
        active_area <= (h_cnt < 640) and (v_cnt < 480);
        
        -- Vertical Blanking
        if v_cnt >= 480 then
          in_vblank <= '1';
        else
          in_vblank <= '0';
        end if;
      end if;
    end if;
  end process;

  -- 4x Hardware Pixel Scaling Address Logic
  pixel_x <= h_cnt / 4 when h_cnt < 640 else 0;
  pixel_y <= v_cnt / 4 when v_cnt < 480 else 0;
  vram_raddr <= to_unsigned(pixel_y * 160 + pixel_x, 15);

  -- Framebuffer Dual-Port Read / Write Process
  process(clk_i)
    variable byte_addr : integer range 0 to 19199;
  begin
    if rising_edge(clk_i) then
      -- Port A: Bus Writes
      if bus_we_i = '1' and bus_addr_i(16) = '0' then
        byte_addr := to_integer(unsigned(bus_addr_i(14 downto 0)));
        if byte_addr < 19200 then
          case bus_addr_i(1 downto 0) is
            when "00" => vram(byte_addr) <= bus_wdata_i(7 downto 0);
            when "01" => vram(byte_addr) <= bus_wdata_i(15 downto 8);
            when "10" => vram(byte_addr) <= bus_wdata_i(23 downto 16);
            when others => vram(byte_addr) <= bus_wdata_i(31 downto 24);
          end case;
        end if;
      end if;

      -- Port B: VGA Scanout Read (synchronous BRAM access)
      if to_integer(vram_raddr) < 19200 then
        pixel_idx <= vram(to_integer(vram_raddr));
      else
        pixel_idx <= (others => '0');
      end if;

      -- Pipeline active display enable matching the 2-cycle read latency
      active_d1 <= active_area;
      active_d2 <= active_d1;
    end if;
  end process;

  -- Palette CLUT Write & Read Process
  process(clk_i)
    variable pal_idx : integer range 0 to 255;
  begin
    if rising_edge(clk_i) then
      -- Palette Write (Addr 0x10000 - 0x103FF)
      if bus_we_i = '1' and bus_addr_i(16) = '1' and bus_addr_i(15 downto 10) = "000000" then
        pal_idx := to_integer(unsigned(bus_addr_i(9 downto 2)));
        palette_mem(pal_idx) <= bus_wdata_i(11 downto 0);
      end if;

      -- Palette Lookup for raster scan
      rgb_color <= palette_mem(to_integer(unsigned(pixel_idx)));
    end if;
  end process;

  -- MMIO Read Interface
  process(bus_addr_i, in_vblank)
  begin
    bus_rdata_o <= (others => '0');
    if bus_addr_i(16) = '1' and bus_addr_i(10) = '1' then
      -- Status register (0x10400): [0] in_vblank
      bus_rdata_o(0) <= in_vblank;
    end if;
  end process;

  -- Final DAC Output Mapping with Blanking (pipelined to match BRAM & CLUT)
  vga_hsync_o <= hsync_reg;
  vga_vsync_o <= vsync_reg;

  vga_red_o   <= rgb_color(11 downto 8) when active_d2 else (others => '0');
  vga_green_o <= rgb_color(7 downto 4)  when active_d2 else (others => '0');
  vga_blue_o  <= rgb_color(3 downto 0)  when active_d2 else (others => '0');

end architecture;
