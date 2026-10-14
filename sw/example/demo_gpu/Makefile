# Application makefile for GEMMrv GPU Standalone demo
MARCH = rv32im_zicsr_zifencei
EFFORT = -Os

USER_FLAGS += -ggdb -gdwarf-3
USER_FLAGS += -Wl,--defsym,__neorv32_rom_size=128k
USER_FLAGS += -Wl,--defsym,__neorv32_ram_size=128k

# Additional sources: GEMM driver, GPU driver, PS/2 driver
APP_SRC = main.c ../gemm_lib/gemmrv_gpu.c ../gemm_lib/gemmrv_hw.c
APP_INC += -I . -I ../gemm_lib

NEORV32_HOME ?= ../../..

include $(NEORV32_HOME)/sw/common/common.mk
