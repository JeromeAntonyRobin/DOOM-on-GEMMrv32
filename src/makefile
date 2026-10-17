# Application makefile for GEMMrvStudio demo_vga_doom
MARCH = rv32im_zicsr_zifencei
EFFORT = -Os

USER_FLAGS += -ggdb -gdwarf-3
USER_FLAGS += -Wl,--gc-sections
USER_FLAGS += -Wl,--defsym,__neorv32_rom_size=32768
USER_FLAGS += -Wl,--defsym,__neorv32_ram_size=98304

APP_SRC = main.c doom_assets_authentic.c doom_map.c r_bsp.c r_segs.c p_mobj.c gemm_lib/gemmrv_gpu.c gemm_lib/gemmrv_hw.c
APP_INC += -I . -I gemm_lib

NEORV32_HOME ?= ../hardware/gemmrv32

include $(NEORV32_HOME)/sw/common/common.mk
