# ==============================================================================
# DOOM-on-GEMMrv32: Top-Level Project Makefile
# ==============================================================================

RISCV_PREFIX ?= riscv64-unknown-elf-
VIVADO       ?= vivado

.PHONY: all v1 v2 v3 flash-bit flash-spi clean help

all: v2

v2:
	@echo "=== Building DOOM v2 (Enhanced Textured 3D Engine) ==="
	$(MAKE) -C src/v2 RISCV_PREFIX=$(RISCV_PREFIX) elf
	@echo "SUCCESS: Output binary verified in bin/demo_vga_doom_v2.bin"

v1:
	@echo "=== Building DOOM v1 (Baseline Flat-Shaded Raycaster) ==="
	$(MAKE) -C src/v1 RISCV_PREFIX=$(RISCV_PREFIX) elf
	@echo "SUCCESS: Output binary verified in bin/demo_vga_doom_v1.bin"

v3:
	@echo "=== Building DOOM v3 (Native 320x200 4bpp BSP Engine) ==="
	$(MAKE) -C src/v3 RISCV_PREFIX=$(RISCV_PREFIX) elf
	@echo "SUCCESS: Output binary verified in bin/demo_vga_doom_v3.bin"

flash-bit:
	@echo "=== Programming Basys 3 FPGA Bitstream via JTAG ==="
	$(VIVADO) -mode batch -source bit/program_jtag.tcl

flash-spi:
	@echo "=== Flashing Basys 3 QSPI Non-Volatile Memory ==="
	$(VIVADO) -mode batch -source bit/flash_basys3.tcl

clean:
	$(MAKE) -C src/v1 RISCV_PREFIX=$(RISCV_PREFIX) clean_all
	$(MAKE) -C src/v2 RISCV_PREFIX=$(RISCV_PREFIX) clean_all
	$(MAKE) -C src/v3 RISCV_PREFIX=$(RISCV_PREFIX) clean_all

help:
	@echo "DOOM-on-GEMMrv32 Build System"
	@echo "----------------------------"
	@echo "  make (or make v2)  - Build DOOM v2 (Enhanced Textured DOOM, default)"
	@echo "  make v1            - Build DOOM v1 (Baseline DDA Raycaster)"
	@echo "  make v3            - Build DOOM v3 (Native 320x200 BSP Engine)"
	@echo "  make flash-bit     - Program bit/neorv32_basys3.bit onto Basys 3 FPGA via JTAG"
	@echo "  make flash-spi     - Program bit/neorv32_basys3.bin to onboard SPI flash"
	@echo "  make clean         - Remove all compiler build output"
