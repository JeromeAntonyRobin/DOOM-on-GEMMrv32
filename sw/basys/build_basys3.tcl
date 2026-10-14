# Resolve paths dynamically relative to this script directory (sw/basys)
set script_dir [file normalize [file dirname [info script]]]
set neorv32_home [file normalize "$script_dir/../.."]
set build_dir [file normalize "$script_dir/build"]
file mkdir $build_dir
set_param general.maxThreads 8

# 1. Create in-memory project
create_project -in_memory -part xc7a35tcpg236-1

# 2. Read package first
read_vhdl -library neorv32 "$neorv32_home/rtl/core/neorv32_package.vhd"

# 3. Read added GEMM modules
read_vhdl -library neorv32 "$neorv32_home/rtl/core/neorv32_wide_bram.vhd"
read_vhdl -library neorv32 "$neorv32_home/rtl/core/neorv32_memory_crossbar.vhd"
read_vhdl -library neorv32 "$neorv32_home/rtl/core/neorv32_cfs.vhd"

# 4. Read core VHDL files from file_list_core.f
set fp [open "$neorv32_home/rtl/file_list_core.f" r]
set file_data [read $fp]
close $fp

set lines [split $file_data "\n"]
foreach line $lines {
    set line [string trim $line]
    if {$line ne ""} {
        set path [string map [list {$NEORV32_HOME} $neorv32_home] $line]
        if {[file exists $path]} {
            read_vhdl -library neorv32 $path
        }
    }
}

# 5. Read Graphics & PS/2 Peripheral Cores
read_vhdl -library work "$neorv32_home/rtl/core/neorv32_vga.vhd"
read_vhdl -library work "$neorv32_home/rtl/core/neorv32_ps2.vhd"

# 6. Read Basys 3 top wrapper and constraints
read_vhdl -library work "$script_dir/neorv32_basys3_top.vhd"
read_xdc "$script_dir/basys3.xdc"

# 7. Synthesize design
synth_design -top neorv32_basys3_top -part xc7a35tcpg236-1 -flatten_hierarchy rebuilt

# 8. Implementation
opt_design
place_design
phys_opt_design
route_design

# 9. Reports
report_utilization -file "$build_dir/utilization.rpt"
report_timing_summary -file "$build_dir/timing_summary.rpt"

# 10. Write bitstream
write_bitstream -force "$build_dir/neorv32_basys3.bit"

# 11. Write SPI Flash Memory Configuration File (.bin) for onboard QSPI flash
write_cfgmem -format bin -size 4 -interface SPIx4 -loadbit "up 0x0 $build_dir/neorv32_basys3.bit" -force "$build_dir/neorv32_basys3.bin"

# 12. Program FPGA immediately via JTAG if connected
puts "PROGRAMMING_JTAG..."
catch {
    open_hw_manager
    connect_hw_server
    open_hw_target
    set dev [get_hw_devices xc7a35t_0]
    current_hw_device $dev
    set_property PROGRAM.FILE "$build_dir/neorv32_basys3.bit" $dev
    program_hw_devices $dev
    puts "JTAG_PROGRAM_SUCCESS"
    close_hw_target
    disconnect_hw_server
    close_hw_manager
}

puts "BUILD_SUCCESSFUL"
exit
