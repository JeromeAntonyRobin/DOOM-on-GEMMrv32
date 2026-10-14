open_hw_manager
connect_hw_server
open_hw_target
set dev [lindex [get_hw_devices] 0]
current_hw_device $dev
refresh_hw_device $dev
boot_hw_device $dev
puts "FPGA_RESET_BOOTLOADER_TRIGGERED"
close_hw_target
disconnect_hw_server
exit
