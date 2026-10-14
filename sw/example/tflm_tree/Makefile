RISCV_PREFIX ?= /mnt/FPGA_DISK/Xilinx/Vitis/2024.2/gnu/riscv/lin/riscv64-unknown-elf/bin/riscv64-unknown-elf-
MARCH = rv32im_zicsr_zifencei
EFFORT = -O3
USER_FLAGS += -ggdb -gdwarf-3
USER_FLAGS += -Wl,--defsym,__neorv32_rom_size=128k
USER_FLAGS += -Wl,--defsym,__neorv32_ram_size=128k

# Use find to get all source files
APP_SRC = $(shell find . -type f -name "*.c" -o -name "*.cpp" -o -name "*.S")
# Add includes
APP_INC = -I . -I third_party/gemmlowp -I third_party/flatbuffers/include -I third_party/ruy

NEORV32_HOME ?= ../../..
include $(NEORV32_HOME)/sw/common/common.mk
