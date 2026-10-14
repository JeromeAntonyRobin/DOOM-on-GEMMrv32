# Basys 3 FPGA Port for GEMMrv32

This directory contains the board-support package, constraint files, synthesis/implementation scripts, and flashing utilities to target the **Digilent Basys 3 FPGA (Xilinx Artix-7 XC7A35T-1CPG236C)**.

---

## Directory Contents

| File | Description |
|---|---|
| `neorv32_basys3_top.vhd` | Top-level VHDL wrapper. Configures the NEORV32 SoC with GEMMrv32 systolic array, 256-bit DMA, 128KB IMEM/DMEM, 50 MHz MMCM clock derivation from 100 MHz oscillator, reset conditioning, and UART/GPIO routing. |
| `basys3.xdc` | Pin constraints for Basys 3: 100 MHz clock (`W5`), reset button (`U18`), LEDs `LD0`..`LD7`, and USB-UART (`B18` Rx, `A18` Tx). Configures Quad-SPI flash bitstream generation (`SPIx4`, 33 MHz). |
| `build_basys3.tcl` | Vivado non-project mode synthesis and implementation script. Synthesizes the core + GEMM CFS, routes the design, generates timing/utilization reports, and exports both `.bit` and SPI Flash `.bin` configuration files. |
| `flash_basys3.tcl` | Hardware manager automation script to program the onboard Quad-SPI Flash memory (Macronix `MX25L3273F` / `MX25L3233F`, 32 Mbit / 4 MB) for non-volatile execution on boot. |
| `reset_fpga.tcl` | Quick Vivado TCL helper to pulse reset or re-initialize the FPGA via JTAG. |
| `upload_firmware.py` | Standalone Python utility to interact with the NEORV32 bootloader over UART (`COM10` @ 19200 baud) to upload executable binary images directly to RAM. |

---

## Hardware Configuration on Basys 3

- **FPGA Part**: Xilinx Artix-7 `xc7a35tcpg236-1`
- **Clock**: On-board 100 MHz oscillator routed through 7-series `MMCME2_BASE` primitive down to **50 MHz** global system clock.
- **NEORV32 ISA**: `rv32im_zicsr_zifencei`
- **Internal Memory**: 128 KB Instruction BRAM (IMEM) + 128 KB Data BRAM (DMEM).
- **GEMM Accelerator**: 8x8 Systolic Array (64 DSP/MAC units) connected to 256-bit wide DMA crossbar via Custom Functions Subsystem (CFS).
- **Peripherals**: UART0 (19200 baud, 8N1), GPIO (8 LEDs).

---

## How to Build Bitstream & SPI Flash Image

Run Vivado in batch mode pointing to `build_basys3.tcl`:

```bash
vivado -mode batch -source sw/basys/build_basys3.tcl
```

Outputs will be generated in `sw/basys/build/`:
- `neorv32_basys3.bit` (SRAM configuration bitstream)
- `neorv32_basys3.bin` (SPI Flash binary image)
- `utilization.rpt` & `timing_summary.rpt`

---

## Flashing the Basys 3 Board (Non-Volatile)

To permanently program the Basys 3 onboard SPI flash so that the GEMMrv32 SoC and bootloader load automatically on power-up:

1. Connect the Basys 3 board via micro-USB and ensure the programming jumper is set to **QSPI**.
2. Run:
```bash
vivado -mode batch -source sw/basys/flash_basys3.tcl
```
3. Once flashing completes, the FPGA boots into the NEORV32 bootloader. Connect your terminal or GEMMrvStudio to the serial port (e.g. `COM10` @ 19200 baud) to interact with the system.
