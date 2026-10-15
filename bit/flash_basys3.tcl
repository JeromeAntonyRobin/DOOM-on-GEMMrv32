# flash_basys3.tcl - Flash Macronix mx25l3273f on Basys 3
open_hw_manager
connect_hw_server
open_hw_target

set device [get_hw_devices xc7a35t_0]
current_hw_device $device
refresh_hw_device -update_hw_probes false $device

puts "--- ATTACHING CONFIGURATION MEMORY (MACRONIX mx25l3273f) ---"
if {[get_property PROGRAM.HW_CFGMEM $device] ne ""} {
    delete_hw_cfgmem [get_property PROGRAM.HW_CFGMEM $device]
}

create_hw_cfgmem -hw_device $device [lindex [get_cfgmem_parts {mx25l3273f-spi-x1_x2_x4}] 0]

set script_dir [file normalize [file dirname [info script]]]
set bin_file [file normalize "$script_dir/neorv32_basys3.bin"]

set cfgmem [get_property PROGRAM.HW_CFGMEM $device]
set_property PROGRAM.UNUSED_PIN_TERMINATION {pull-none} $cfgmem
set_property PROGRAM.BLANK_CHECK  0 $cfgmem
set_property PROGRAM.ERASE        1 $cfgmem
set_property PROGRAM.CFG_PROGRAM  1 $cfgmem
set_property PROGRAM.VERIFY       1 $cfgmem
set_property PROGRAM.CHECKSUM     0 $cfgmem
set_property PROGRAM.ADDRESS_RANGE {use_file} $cfgmem
set_property PROGRAM.FILES [list $bin_file] $cfgmem

puts "--- PROGRAMMING SPI FLASH MEMORY ---"
create_hw_bitstream -hw_device $device [get_property PROGRAM.HW_CFGMEM_BITFILE $device]
program_hw_devices $device
refresh_hw_device $device
program_hw_cfgmem -hw_cfgmem $cfgmem

puts "--- SPI FLASH PROGRAMMED AND VERIFIED SUCCESSFULLY ---"
boot_hw_device $device

close_hw_target
disconnect_hw_server
close_hw_manager
puts "FLASH_COMPLETE"
exit
