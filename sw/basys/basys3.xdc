## Clock signal
set_property PACKAGE_PIN W5 [get_ports clk_i]
set_property IOSTANDARD LVCMOS33 [get_ports clk_i]
create_clock -add -name sys_clk_pin -period 10.00 -waveform {0 5} [get_ports clk_i]

## Reset button (Center button btnC on Basys 3: active high on board)
set_property PACKAGE_PIN U18 [get_ports btn_reset]
set_property IOSTANDARD LVCMOS33 [get_ports btn_reset]

## LEDs (LD0..LD7)
set_property PACKAGE_PIN U16 [get_ports {gpio_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[0]}]
set_property PACKAGE_PIN E19 [get_ports {gpio_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[1]}]
set_property PACKAGE_PIN U19 [get_ports {gpio_o[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[2]}]
set_property PACKAGE_PIN V19 [get_ports {gpio_o[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[3]}]
set_property PACKAGE_PIN W18 [get_ports {gpio_o[4]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[4]}]
set_property PACKAGE_PIN U15 [get_ports {gpio_o[5]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[5]}]
set_property PACKAGE_PIN U14 [get_ports {gpio_o[6]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[6]}]
set_property PACKAGE_PIN V14 [get_ports {gpio_o[7]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_o[7]}]

## USB-RS232 Interface
## Basys 3: B18 is Rx (FPGA receives from PC Tx), A18 is Tx (FPGA transmits to PC Rx)
set_property PACKAGE_PIN B18 [get_ports uart0_rxd_i]
set_property IOSTANDARD LVCMOS33 [get_ports uart0_rxd_i]
set_property PACKAGE_PIN A18 [get_ports uart0_txd_o]
set_property IOSTANDARD LVCMOS33 [get_ports uart0_txd_o]

## VGA Connector (12-bit resistor DAC: 4-bit Red, 4-bit Green, 4-bit Blue + Sync)
set_property PACKAGE_PIN G19 [get_ports {vga_red_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_red_o[0]}]
set_property PACKAGE_PIN H19 [get_ports {vga_red_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_red_o[1]}]
set_property PACKAGE_PIN J19 [get_ports {vga_red_o[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_red_o[2]}]
set_property PACKAGE_PIN N19 [get_ports {vga_red_o[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_red_o[3]}]

set_property PACKAGE_PIN N18 [get_ports {vga_blue_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_blue_o[0]}]
set_property PACKAGE_PIN L18 [get_ports {vga_blue_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_blue_o[1]}]
set_property PACKAGE_PIN K18 [get_ports {vga_blue_o[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_blue_o[2]}]
set_property PACKAGE_PIN J18 [get_ports {vga_blue_o[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_blue_o[3]}]

set_property PACKAGE_PIN J17 [get_ports {vga_green_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_green_o[0]}]
set_property PACKAGE_PIN H17 [get_ports {vga_green_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_green_o[1]}]
set_property PACKAGE_PIN G17 [get_ports {vga_green_o[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_green_o[2]}]
set_property PACKAGE_PIN D17 [get_ports {vga_green_o[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {vga_green_o[3]}]

set_property PACKAGE_PIN P19 [get_ports vga_hsync_o]
set_property IOSTANDARD LVCMOS33 [get_ports vga_hsync_o]
set_property PACKAGE_PIN R19 [get_ports vga_vsync_o]
set_property IOSTANDARD LVCMOS33 [get_ports vga_vsync_o]

## USB-HID / PS2 Keyboard Connector (Basys 3 PIC24 Host Bridge)
set_property PACKAGE_PIN C17 [get_ports ps2_clk_i]
set_property IOSTANDARD LVCMOS33 [get_ports ps2_clk_i]
set_property PULLUP true [get_ports ps2_clk_i]
set_property PACKAGE_PIN B17 [get_ports ps2_data_i]
set_property IOSTANDARD LVCMOS33 [get_ports ps2_data_i]
set_property PULLUP true [get_ports ps2_data_i]

## Configuration SPI Flash properties for Basys 3 (Spansion S25FL032P / Macronix MX25L3233F)
set_property CONFIG_MODE SPIx4 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property BITSTREAM.CONFIG.CONFIGRATE 33 [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
