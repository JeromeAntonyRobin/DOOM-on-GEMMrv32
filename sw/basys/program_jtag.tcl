set script_dir [file normalize [file dirname [info script]]]
set bit_file [file normalize "$script_dir/build/neorv32_basys3.bit"]

open_hw_manager
connect_hw_server
open_hw_target
set dev [lindex [get_hw_devices xc7a35t_*] 0]
current_hw_device $dev
set_property PROGRAM.FILE $bit_file $dev
program_hw_devices $dev
puts "BITSTREAM_FLASH_SUCCESS: $bit_file loaded onto Basys 3"
close_hw_target
disconnect_hw_server
close_hw_manager
exit
