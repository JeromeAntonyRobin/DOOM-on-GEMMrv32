open_hw_manager
connect_hw_server
open_hw_target
set dev [lindex [get_hw_devices xc7a35t_*] 0]
current_hw_device $dev
set_property PROGRAM.FILE {C:/Users/Lenovo/Desktop/GEMMrv32/sw/basys/build/neorv32_basys3.bit} $dev
program_hw_devices $dev
puts "BITSTREAM_FLASH_SUCCESS"
close_hw_target
disconnect_hw_server
close_hw_manager
exit
