# GEMMrv32 DOOM: Bare-Metal 3D DOOM Engine on Digilent Basys 3 FPGA

[![Target Platform](https://img.shields.io/badge/FPGA-Digilent%20Basys%203%20(XC7A35T)-orange?style=flat-square)](https://digilent.com/reference/programmable-logic/basys-3/start)
[![ISA](https://img.shields.io/badge/ISA-RV32IM-green?style=flat-square)](https://riscv.org)
[![System Clock](https://img.shields.io/badge/Clock-50.0%20MHz%20MMCM-blue?style=flat-square)]()
[![Video Output](https://img.shields.io/badge/VGA-640x480%20%40%2060Hz%20(12--bit%20RGB)-purple?style=flat-square)]()
[![Author](https://img.shields.io/badge/Author-JeromeAntonyRobin-black?style=flat-square)](https://github.com/JeromeAntonyRobin)

An authentic, hardware-accelerated bare-metal port of the **Id Software 3D DOOM graphics pipeline** running on the **NEORV32 RISC-V SoC** on a **Digilent Basys 3 FPGA (`xc7a35tcpg236-1`)**. Features custom hardware VGA scanout, debounced PS/2 keyboard interface, and an **8x8 Systolic GEMM Coprocessor** executing parallel 3D matrix transformations in hardware.

---

## Branch Overview: `basys-doom`

This branch contains the **complete DOOM gaming workstation**, including the FPGA bitstream, pure Vivado TCL scripts, hardware peripherals, and all three engine generations.

```
+-----------------------------------------------------------------------------------+
|                        BASYS 3 DOOM SYSTEM ARCHITECTURE                          |
|                                                                                   |
|   +---------------------------------------------------------------------------+   |
|   |                        NEORV32 RV32IM CPU @ 50 MHz                        |   |
|   |            64 KB Instruction SRAM (IMEM)  |  64 KB Data SRAM (DMEM)       |   |
|   +---------------------------------------------------------------------------+   |
|          │                                                       │                |
|          │ CFS Coprocessor Port (0xFFEA0000)                     │ Wishbone / XBUS|
|          ▼                                                       ▼                |
|   +--------------------------+                         +----------------------+   |
|   |  8x8 Systolic GEMM Unit  |                         |  VGA Framebuffer &   |   |
|   |  - 256-bit Wide DMA      |                         |  12-bit Palette Core |   |
|   |  - 66x Vector Speedup    |                         |  - 19,200 B Dual-Port|   |
|   |  - 3D Monster Projection |                         |  - 4x HW Scaler 60Hz |   |
|   +--------------------------+                         +----------------------+   |
|                                                                  │                |
|                                                                  ▼                |
|                                                            12-bit RGB DAC         |
|                                                            VGA Monitor (J1)       |
+-----------------------------------------------------------------------------------+
```

---

## The Three DOOM Engine Releases

All three versions are located in [`sw/example/`](sw/example/) and compile cleanly with bare-metal RISC-V GCC:

| Release | Directory | Key Features | Memory Utilization | Verified Binary |
|---|---|---|---|:---:|
| **DOOM v1** | [`sw/example/demo_vga_doom_v1/`](sw/example/demo_vga_doom_v1/) | **Baseline DDA Raycaster**: Butter-smooth 60 FPS, single-pass vertical column rasterizer (Ceiling $\to$ Shaded Wall $\to$ Floor). | `9,332 B` (14% IMEM, 0.2% DMEM) | [`demo_vga_doom_v1.bin`](sw/example/demo_vga_doom_v1/demo_vga_doom.bin) |
| **DOOM v2** | [`sw/example/demo_vga_doom_v2/`](sw/example/demo_vga_doom_v2/) | **Enhanced Retro DOOM (Option A)**: 4x high-resolution textures, 1D Z-Buffer, 8x8 GEMM-projected dynamic monsters, bobbing Shotgun with muzzle flash, and Doomguy status bar HUD. **Guaranteed zero ghosting / 100% full-frame refresh.** | `21,300 B` (21% IMEM, 12% DMEM) | [`demo_vga_doom_v2.bin`](sw/example/demo_vga_doom_v2/demo_vga_doom_v2.bin) |
| **DOOM v3** | [`sw/example/demo_vga_doom_v3/`](sw/example/demo_vga_doom_v3/) | **Native 320x200 @ 4bpp BSP Engine**: Authentic 1993 Id Software architecture with arbitrary linedefs, subsectors, front-to-back recursive BSP node traversal, and dynamic mobjs. | `41,244 B` (27% IMEM, 36% DMEM) | [`demo_vga_doom_v3.bin`](sw/example/demo_vga_doom_v3/demo_vga_doom_v3.bin) |

---

## Hardware Modules Added

* **VGA Rasterizer (`rtl/core/neorv32_vga.vhd`)**: Dual-port VRAM, 256-color 12-bit RGB Color Lookup Table (CLUT), and 4x nearest-neighbor hardware scaler upscaling $160 \times 120$ to physical $640 \times 480$ @ 60 Hz.
* **PS/2 Keyboard Core (`rtl/core/neorv32_ps2.vhd`)**: Synchronous 100 kHz digital deglitching filter, 11-bit frame decoder, and 16-byte hardware FIFO.
* **8x8 Systolic GEMM Coprocessor (`rtl/core/neorv32_cfs.vhd`)**: Memory-mapped coprocessor at `0xFFEA0000` with 256-bit DMA interconnect. Dispatches 8 dynamic entities in camera space in under 85 clock cycles ($66\times$ faster than software CPU multiplication).

---

## Physical Basys 3 Board Configuration

1. **`JP1` (Programming Mode Selector)**:
   * **Set to `JTAG` (bridge the center two pins).**
   * *Critical:* If set to `USB`, the host port enters thumb-drive mode and blocks keyboard input.
2. **`JP2` / `JP3` (Power Source Selector)**:
   * **Set to `USB`** (power drawn from micro-USB cable `J4`).
3. **Buttons**:
   * **`btnC` (Center push button `U18`):** Soft CPU Reset (restarts the NEORV32 bootloader without wiping FPGA SRAM).
   * **`PROG` (Red button near top-left):** Reconfigures the FPGA.

---

## 1-Click Flashing & Execution (Pure Vivado TCL)

No OS-specific scripts required. Run natively in Vivado batch mode:

### 1. Program Hardware Bitstream (JTAG)
The pre-synthesized bitstream [`sw/basys/build/neorv32_basys3.bit`](sw/basys/build/neorv32_basys3.bit) is tracked directly in Git:
```bash
vivado -mode batch -source sw/basys/program_jtag.tcl
```

### 2. Synthesize From Source (Optional)
To re-synthesize and implement the complete hardware from source:
```bash
vivado -mode batch -source sw/basys/build_basys3.tcl
```

### 3. Upload Game Firmware
Connect to the Basys 3 USB-UART port (`COM10` or `/dev/ttyUSB*`) at **19200 baud, 8N1**:
* Press **`btnC`** to enter the NEORV32 bootloader.
* Press `u` on your terminal, then send [`demo_vga_doom_v2.bin`](sw/example/demo_vga_doom_v2/demo_vga_doom_v2.bin).
* Press `e` to execute!

---

## Controls & Input

Dual-mode input is active across both the **Host PC Keyboard** (via serial terminal @ 19200 baud) and the **Basys 3 PS/2 Port (`J2`)**:

| Key | Action | Engine Response |
|:---:|:---:|:---|
| **`W`** | Move Forward | Moves camera forward along view vector with wall collision & weapon bob |
| **`S`** | Move Backward | Moves camera backward with wall collision & weapon bob |
| **`A`** | Turn Left | Rotates camera direction and projection plane left |
| **`D`** | Turn Right | Rotates camera direction and projection plane right |
| **`SPACE` / `F`** | **Fire Shotgun!** | Fires twin barrels, triggers 2-frame muzzle blast, spends ammo, and makes Doomguy grin |
| **`M`** | Toggle Minimap | Shows / hides the 32x32 top-left radar overlay |
| **`R`** | Respawn / Reset | Resets player position to spawn point `(7.5, 9.5)` and restores health/ammo |

---

## Technical Documentation Pointers

* **Master Hardware Specification:** [`docs/GEMMRV32_SYSTEM_SPECIFICATION.md`](docs/GEMMRV32_SYSTEM_SPECIFICATION.md)
  * Section 3: Hardware Memory Architecture & Register Maps
  * Section 6.1: The 32-bit vs 8-bit VRAM Write Ghosting Anomaly & Resolution
  * **Section 6.2: The DOOM v3 4bpp Stride, Write-Only Bus Trap & Hall-of-Mirrors Glitch**
* **3D Graphics & GPU Design:** [`docs/DOOM_GRAPHICS_PIPELINE.md`](docs/DOOM_GRAPHICS_PIPELINE.md)
* **Bootloader & UART Timing Debug:** [`docs/HARDWARE_DEBUG_REPORT.md`](docs/HARDWARE_DEBUG_REPORT.md)

---

## Repository Branch Directory

* [`master`](https://github.com/JeromeAntonyRobin/GEMMPrivate/tree/master): Main index and baseline repository snapshot.
* [`pure-rtl`](https://github.com/JeromeAntonyRobin/GEMMPrivate/tree/pure-rtl): Board-agnostic VHDL core IP library (NEORV32 + GEMM + DMA + VGA + PS/2).
* [`nexys`](https://github.com/JeromeAntonyRobin/GEMMPrivate/tree/nexys): Digilent Nexys A7 100T TinyML platform with TensorFlow Lite Micro & LeNet.
* [`basys`](https://github.com/JeromeAntonyRobin/GEMMPrivate/tree/basys): Clean Basys 3 SoC baseline with tracked bitstream and standard bringup demos.
* [`basys-doom`](https://github.com/JeromeAntonyRobin/GEMMPrivate/tree/basys-doom): **(You are here)** Complete 3D DOOM gaming platform.
