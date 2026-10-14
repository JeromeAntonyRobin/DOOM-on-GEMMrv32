library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

library unisim;
use unisim.vcomponents.all;

library neorv32;
use neorv32.neorv32_package.all;

entity neorv32_basys3_top is
  generic (
    CLOCK_FREQUENCY : natural := 50000000;  -- 50 MHz clock for NEORV32
    IMEM_SIZE       : natural := 64*1024;   -- 64 KB IMEM (16 RAMB36)
    DMEM_SIZE       : natural := 64*1024    -- 64 KB DMEM (16 RAMB36)
  );
  port (
    -- 100 MHz oscillator on Basys 3 (pin W5)
    clk_i       : in  std_ulogic;
    -- Center push button (active high on pin U18)
    btn_reset   : in  std_ulogic;
    -- 8 onboard LEDs
    gpio_o      : out std_ulogic_vector(7 downto 0);
    -- UART (B18=Rx, A18=Tx)
    uart0_txd_o : out std_ulogic;
    uart0_rxd_i : in  std_ulogic;

    -- VGA 12-bit resistor DAC + Sync (Basys 3 connector)
    vga_red_o   : out std_ulogic_vector(3 downto 0);
    vga_green_o : out std_ulogic_vector(3 downto 0);
    vga_blue_o  : out std_ulogic_vector(3 downto 0);
    vga_hsync_o : out std_ulogic;
    vga_vsync_o : out std_ulogic;

    -- PS/2 Keyboard Interface (Basys 3 PIC24 Host Bridge)
    ps2_clk_i   : in  std_ulogic;
    ps2_data_i  : in  std_ulogic
  );
end entity;

architecture rtl of neorv32_basys3_top is

  signal clk_50m      : std_ulogic;
  signal clk_50m_unbuf: std_logic;
  signal clk_fb_unbuf : std_logic;
  signal clk_fb       : std_logic;
  signal mmcm_locked  : std_logic;
  signal rstn_sync    : std_ulogic := '0';
  signal rstn_ff1     : std_ulogic := '0';
  signal con_gpio_out : std_ulogic_vector(31 downto 0);

  -- Wishbone / XBUS signals
  signal xbus_adr     : std_ulogic_vector(31 downto 0);
  signal xbus_dat_o   : std_ulogic_vector(31 downto 0);
  signal xbus_we      : std_ulogic;
  signal xbus_sel     : std_ulogic_vector(3 downto 0);
  signal xbus_stb     : std_ulogic;
  signal xbus_cyc     : std_ulogic;
  signal xbus_dat_i   : std_ulogic_vector(31 downto 0);
  signal xbus_ack     : std_ulogic;

  -- Peripheral Selects on XBUS:
  -- Base 0x90000000: VGA Framebuffer (160x120) & Palette (0x90000000 - 0x9001FFFF)
  -- Base 0x90020000: PS/2 Keyboard FIFO (0x90020000 - 0x9002000F)
  signal vga_sel      : boolean;
  signal ps2_sel      : boolean;
  signal vga_rdata    : std_ulogic_vector(31 downto 0);
  signal ps2_rdata    : std_ulogic_vector(31 downto 0);
  signal vga_we       : std_ulogic;
  signal ps2_re       : std_ulogic;

begin

  -- Dedicated 7-series MMCME2 primitive: 100 MHz in -> 50 MHz global clock out
  mmcm_inst : MMCME2_BASE
  generic map (
    BANDWIDTH          => "OPTIMIZED",
    CLKFBOUT_MULT_F    => 10.0,
    CLKFBOUT_PHASE     => 0.0,
    CLKIN1_PERIOD      => 10.0,
    CLKOUT0_DIVIDE_F   => 20.0,
    CLKOUT0_DUTY_CYCLE => 0.5,
    CLKOUT0_PHASE      => 0.0,
    DIVCLK_DIVIDE      => 1,
    REF_JITTER1        => 0.010,
    STARTUP_WAIT       => false
  )
  port map (
    CLKOUT0  => clk_50m_unbuf,
    CLKOUT0B => open,
    CLKOUT1  => open,
    CLKOUT1B => open,
    CLKOUT2  => open,
    CLKOUT2B => open,
    CLKOUT3  => open,
    CLKOUT3B => open,
    CLKOUT4  => open,
    CLKOUT5  => open,
    CLKOUT6  => open,
    CLKFBOUT => clk_fb_unbuf,
    CLKFBOUTB=> open,
    LOCKED   => mmcm_locked,
    CLKIN1   => clk_i,
    PWRDWN   => '0',
    RST      => '0',
    CLKFBIN  => clk_fb
  );

  bufg_fb_inst : BUFG
  port map (
    I => clk_fb_unbuf,
    O => clk_fb
  );

  bufg_inst : BUFG
  port map (
    I => clk_50m_unbuf,
    O => clk_50m
  );

  process(clk_50m, btn_reset, mmcm_locked)
  begin
    if (btn_reset = '1') or (mmcm_locked = '0') then
      rstn_ff1  <= '0';
      rstn_sync <= '0';
    elsif rising_edge(clk_50m) then
      rstn_ff1  <= '1';
      rstn_sync <= rstn_ff1;
    end if;
  end process;

  -- Instantiate NEORV32 GEMMrv32 Top with XBUS enabled for graphics & keyboard
  neorv32_top_inst: entity neorv32.neorv32_top
  generic map (
    CLOCK_FREQUENCY  => CLOCK_FREQUENCY,
    BOOT_MODE_SELECT => 0,                 -- Bootloader mode (UART 'u' upload)
    RISCV_ISA_C      => true,
    RISCV_ISA_M      => true,
    RISCV_ISA_Zicntr => true,
    IMEM_EN          => true,
    IMEM_SIZE        => IMEM_SIZE,
    DMEM_EN          => true,
    DMEM_SIZE        => DMEM_SIZE,
    IO_CFS_EN        => true,              -- 8x8 Systolic GEMM GPU Accelerator
    IO_GPIO_NUM      => 8,
    IO_CLINT_EN      => true,
    IO_UART0_EN      => true,
    IO_UART0_RX_FIFO => 64,                -- 64-byte RX FIFO to prevent dropouts during streaming
    IO_UART0_TX_FIFO => 64,                -- 64-byte TX FIFO
    XBUS_EN          => true,              -- External Bus for VGA & PS/2
    XBUS_TIMEOUT     => 128
  )
  port map (
    clk_i       => clk_50m,
    rstn_i      => rstn_sync,
    gpio_o      => con_gpio_out,
    uart0_txd_o => uart0_txd_o,
    uart0_rxd_i => uart0_rxd_i,
    
    -- XBUS Interconnect
    xbus_adr_o  => xbus_adr,
    xbus_dat_o  => xbus_dat_o,
    xbus_we_o   => xbus_we,
    xbus_sel_o  => xbus_sel,
    xbus_stb_o  => xbus_stb,
    xbus_cyc_o  => xbus_cyc,
    xbus_dat_i  => xbus_dat_i,
    xbus_ack_i  => xbus_ack,
    xbus_err_i  => '0'
  );

  -- Address Decoding on XBUS:
  -- 0x90000000 - 0x9001FFFF : VGA Controller
  -- 0x90020000 - 0x9002FFFF : PS/2 Keyboard
  vga_sel <= (xbus_cyc = '1') and (xbus_stb = '1') and (xbus_adr(31 downto 20) = x"900") and (xbus_adr(17) = '0');
  ps2_sel <= (xbus_cyc = '1') and (xbus_stb = '1') and (xbus_adr(31 downto 20) = x"900") and (xbus_adr(17) = '1');

  vga_we  <= '1' when (vga_sel and xbus_we = '1') else '0';
  ps2_re  <= '1' when (ps2_sel and xbus_we = '0' and xbus_ack = '1') else '0';

  -- 1-cycle Wishbone Acknowledge generator for mapped peripherals
  process(clk_50m, rstn_sync)
  begin
    if rstn_sync = '0' then
      xbus_ack <= '0';
    elsif rising_edge(clk_50m) then
      if (vga_sel or ps2_sel) and (xbus_ack = '0') then
        xbus_ack <= '1';
      else
        xbus_ack <= '0';
      end if;
    end if;
  end process;

  xbus_dat_i <= vga_rdata when vga_sel else
                ps2_rdata when ps2_sel else
                (others => '0');

  -- Instantiate VGA Video Controller
  vga_inst: entity work.neorv32_vga
  port map (
    clk_i       => clk_50m,
    rstn_i      => rstn_sync,
    bus_addr_i  => xbus_adr(16 downto 0),
    bus_wdata_i => xbus_dat_o,
    bus_ben_i   => xbus_sel,
    bus_we_i    => vga_we,
    bus_rdata_o => vga_rdata,
    vga_red_o   => vga_red_o,
    vga_green_o => vga_green_o,
    vga_blue_o  => vga_blue_o,
    vga_hsync_o => vga_hsync_o,
    vga_vsync_o => vga_vsync_o
  );

  -- Instantiate PS/2 Keyboard Controller
  ps2_inst: entity work.neorv32_ps2
  generic map (
    FIFO_DEPTH => 16
  )
  port map (
    clk_i       => clk_50m,
    rstn_i      => rstn_sync,
    bus_addr_i  => xbus_adr(3 downto 0),
    bus_re_i    => ps2_re,
    bus_rdata_o => ps2_rdata,
    ps2_clk_i   => ps2_clk_i,
    ps2_data_i  => ps2_data_i
  );

  gpio_o(7)          <= mmcm_locked;
  gpio_o(6 downto 2) <= con_gpio_out(6 downto 2);
  gpio_o(1)          <= ps2_data_i;  -- LD1: ON when data line idle (pullup), flickers on activity
  gpio_o(0)          <= ps2_clk_i;   -- LD0: ON when clock line idle (pullup), flickers on activity

end architecture;
