# ==============================================================================
# DOOM-on-GEMMrv32: Top-Level Project Makefile
# ==============================================================================

RISCV_PREFIX ?= riscv64-unknown-elf-
OPENFPGALOADER ?= openFPGALoader
PYTHON         ?= python3
SERIAL_PORT    ?= /dev/ttyUSB1
BAUD           ?= 19200

.PHONY: all build flash-bit flash-spi upload clean help

all: build

build:
	@echo "=== Building DOOM (Authentic BSP + GEMM Coprocessor) ==="
	$(MAKE) -C src RISCV_PREFIX=$(RISCV_PREFIX) elf
	$(RISCV_PREFIX)objcopy -I elf32-littleriscv -O binary src/main.elf src/main_raw.bin
	$(PYTHON) tools/image_gen.py -i src/main_raw.bin -o bin/demo_vga_doom.bin
	@echo "SUCCESS: Output binary generated in bin/demo_vga_doom.bin"

flash-bit:
	@tools/flash_bitstream.sh bit/neorv32_basys3.bit

flash-spi:
	@echo "=== Flashing Basys 3 Non-Volatile SPI Flash via openFPGALoader ==="
	sudo $(OPENFPGALOADER) --board=basys3 -f bit/neorv32_basys3.bin

upload:
	@tools/flash_firmware.sh $(SERIAL_PORT) bin/demo_vga_doom.bin

clean:
	$(MAKE) -C src RISCV_PREFIX=$(RISCV_PREFIX) clean_all

help:
	@echo "DOOM-on-GEMMrv32 Open-Source Build & Flash System"
	@echo "-------------------------------------------------"
	@echo "  make (or make build) - Compile game binary using RISC-V GCC"
	@echo "  make flash-bit       - Program FPGA bitstream directly into SRAM (openFPGALoader)"
	@echo "  make flash-spi       - Program bitstream into SPI Flash (openFPGALoader)"
	@echo "  make upload          - Upload bin/demo_vga_doom.bin to Basys 3 via UART loader"
	@echo "  make clean           - Remove compiler build output"
