# Application makefile.
MARCH = rv32im_zicsr_zifencei

# Path to RISC-V GCC toolchain from Vivado installation
RISCV_PREFIX ?= D:/vivado/2025.2/gnu/riscv/nt/bin/riscv64-unknown-elf-

EFFORT = -Os
USER_FLAGS += -ggdb -gdwarf-3
USER_FLAGS += -Wl,--defsym,__neorv32_rom_size=128k
USER_FLAGS += -Wl,--defsym,__neorv32_ram_size=128k

APP_SRC = main.c ../gemm_lib/gemmrv_hw.c
APP_INC += -I .

NEORV32_HOME ?= ../../..
include $(NEORV32_HOME)/sw/common/common.mk
