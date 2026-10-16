# DOOM on GEMMrv32: Bare-Metal 3D DOOM on Digilent Basys 3 FPGA

[![Target FPGA](https://img.shields.io/badge/FPGA-Digilent%20Basys%203%20(XC7A35T)-orange?style=flat-square)](https://digilent.com/reference/programmable-logic/basys-3/start)
[![ISA](https://img.shields.io/badge/ISA-RV32IM%20RISC--V-green?style=flat-square)](https://riscv.org)
[![Clock](https://img.shields.io/badge/Clock-50.0%20MHz%20MMCM-blue?style=flat-square)]()
[![Video Architecture](https://img.shields.io/badge/Video-640x480%20%40%2060Hz%20(12--bit%20RGB)-purple?style=flat-square)]()
[![Hardware Coprocessor](https://img.shields.io/badge/Coprocessor-8x8%20Systolic%20GEMM-brightgreen?style=flat-square)]()
[![Author](https://img.shields.io/badge/Author-JeromeAntonyRobin-black?style=flat-square)](https://github.com/JeromeAntonyRobin)

An authentic, hardware-accelerated bare-metal port of the **Id Software 3D DOOM graphics pipeline** running on the **GEMMrv32 RISC-V SoC** on a **Digilent Basys 3 FPGA (`xc7a35tcpg236-1`)**. Features custom hardware VGA scanout, a debounced PS/2 keyboard receiver, and an **8x8 Systolic-Array GEMM Coprocessor** executing parallel 3D matrix transformations in hardware.

---

## Repository Structure

This repository is strictly organized for **building, understanding, and running DOOM on the GEMMrv32 design**:

```
DOOM-on-GEMMrv32/
├── bit/                    # FPGA Hardware Bitstreams & 1-Click TCL Programmers
│   ├── neorv32_basys3.bit  # Synthesized Basys 3 bitstream (tracked in Git)
│   ├── neorv32_basys3.bin  # Macronix SPI flash configuration binary
│   ├── program_jtag.tcl    # Pure Vivado TCL JTAG programmer
│   └── flash_basys3.tcl    # Pure Vivado TCL SPI flash programmer
│
├── bin/                    # Pre-Compiled, Bootloader-Ready Game Binaries
│   ├── demo_vga_doom_v2.bin# DOOM v2 (Enhanced: 4x Textures, GEMM Monsters, HUD) [RECOMMENDED]
│   ├── demo_vga_doom_v1.bin# DOOM v1 (Baseline: Butter-Smooth Flat-Shaded Raycaster)
│   ├── demo_vga_doom_v3.bin# DOOM v3 (Native 320x200 4bpp BSP Linedef Engine)
│   └── demo_vga_doom.bin   # Default drop-in binary (points to v2)
│
├── src/                    # Bare-Metal C Source Code
│   ├── v1/                 # DOOM v1 source (single-pass DDA raycaster)
│   ├── v2/                 # DOOM v2 source (textured walls, 1D Z-Buffer, HUD)
│   ├── v3/                 # DOOM v3 source (authentic Id Software BSP tree engine)
│   └── gemm_lib/           # Hardware drivers (VGA primitives, GEMM 3D matrix API)
│
├── docs/                   # Architectural & Technical Engineering Specifications
│   ├── DOOM_ENGINE_ARCHITECTURE.md    # 3D raycasting DDA math, textures, & BSP traversal
│   ├── HARDWARE_SPECIFICATION.md      # Memory map, GEMM CFS coprocessor, & VGA core
│   └── DOOM_GLITCH_ANALYSIS.md        # The 19.2 KB boundary & Hall-of-Mirrors glitch resolution
│
├── hardware/gemmrv32/      # Git submodule: Basys 3 SoC HDL from The-Silis-Foundation/GEMMrv32 (branch: basys3)
│
├── sw/                     # Minimal NEORV32 C Runtime & Linker Scripts
│   ├── common/             # Linker script (neorv32.ld), crt0.S startup, common.mk
│   └── lib/                # Peripheral driver headers (neorv32.h, UART, CFS)
│
└── Makefile                # 1-Click Top-Level Makefile (builds v1, v2, v3 & flashes FPGA)
```

---

## Quick Start: Play DOOM in 60 Seconds

You do **not** need to synthesize the FPGA from source. The pre-synthesized bitstream is tracked directly in `bit/`:

### Step 1: Program the FPGA (JTAG)
Connect your Digilent Basys 3 via USB and run:
```bash
make flash-bit
# Or directly via Vivado:
vivado -mode batch -source bit/program_jtag.tcl
```
*The FPGA is configured in ~3 seconds.*

### Step 2: Upload Game Firmware
Connect to the Basys 3 USB-UART port (`COM10` on Windows or `/dev/ttyUSB*` on Linux) at **19200 baud, 8N1**:
1. Press **`btnC`** (center button) on the Basys 3 to enter the bootloader.
2. Press **`u`** in your serial terminal to prepare the UART loader.
3. Send the executable binary: [`bin/demo_vga_doom_v2.bin`](bin/demo_vga_doom_v2.bin).
4. Send **`e`** to start execution.
5. Watch DOOM render live on your VGA monitor!

---

## Controls & Gameplay Guide

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

## The Three DOOM Engine Generations

| Release | Focus & Mechanics | Resolution & Depth | SRAM Footprint | Verified Binary |
|---|---|:---:|---|:---:|
| **DOOM v1** | **Baseline DDA Raycaster**: Butter-smooth 60 FPS, single-pass vertical column rasterizer (Ceiling $\to$ Shaded Wall $\to$ Floor). Zero ghosting, rock-solid baseline. | $160 \times 120$ @ 8bpp | `9,332 B` (14% IMEM / 0.2% DMEM) | [`bin/demo_vga_doom_v1.bin`](bin/demo_vga_doom_v1.bin) |
| **DOOM v2** | **Enhanced Retro DOOM (Option A)**: 4x high-resolution textures, 1D Z-Buffer, 8x8 GEMM-projected dynamic monsters, bobbing Shotgun with muzzle flash, and Doomguy status bar HUD. **Guaranteed zero ghosting / 100% full-frame refresh.** | $160 \times 120$ @ 8bpp | `21,300 B` (21% IMEM / 12% DMEM) | [`bin/demo_vga_doom_v2.bin`](bin/demo_vga_doom_v2.bin) |
| **DOOM v3** | **Native 320x200 @ 4bpp BSP Engine**: Authentic 1993 Id Software architecture with arbitrary linedefs, subsectors, front-to-back recursive BSP node traversal, and dynamic mobjs. | $320 \times 200$ @ 4bpp | `41,244 B` (27% IMEM / 36% DMEM) | [`bin/demo_vga_doom_v3.bin`](bin/demo_vga_doom_v3.bin) |

---

## How It Works: Hardware & Accelerator Architecture

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

### 1. The 8x8 Systolic GEMM Coprocessor in 3D Graphics
Dynamic world entities (Imps, Explosive Barrels, Health Kits) are represented by 3D coordinates. In software, projecting $N$ entities into camera space requires multiple floating- or fixed-point trigonometric multiplications per entity.

The custom hardware coprocessor (`0xFFEA0000`) computes the camera-space projection for all 8 entities in a single systolic burst:

$$\begin{bmatrix} X_{cam} \\ Y_{cam} \\ Z_{cam} \\ 1 \end{bmatrix} = \begin{bmatrix} \cos\theta & -\sin\theta & 0 & 0 \\ \sin\theta & \cos\theta & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix} \times \begin{bmatrix} X_{world} - X_{player} \\ Y_{world} - Y_{player} \\ Z_{world} \\ 1 \end{bmatrix}$$

* The hardware coprocessor executes this 4x4 matrix multiplication for 8 vectors in parallel using its 256-bit wide DMA channel in **under 85 clock cycles** ($66\times$ faster than software RV32IM multiplication).

### 2. Custom Dual-Port VGA Raster Core
* **Framebuffer:** 19,200 bytes of dual-port Block RAM ($160 \times 120$ internal resolution).
* **Palette CLUT:** 256-entry $\times$ 12-bit RGB lookup table.
* **Hardware Upscaler:** Real-time 4x nearest-neighbor hardware scaler upscaling $160 \times 120$ internal resolution to standard $640 \times 480$ @ 60 Hz display timing.

---

## Building from Source

Prerequisites:
* RISC-V GCC toolchain with `rv32im_zicsr_zifencei` support (`riscv64-unknown-elf-gcc`).
* GNU Make.

```bash
# Build DOOM v2 (Enhanced, default)
make v2

# Build DOOM v1 (Baseline flat-shaded)
make v1

# Build DOOM v3 (Native 320x200 BSP engine)
make v3
```

---

## Physical Basys 3 Jumper Configuration

1. **`JP1` (Programming Mode Selector)**:
   * **Set to `JTAG` (bridge the center two pins).**
   * *Critical:* If set to `USB`, the host port enters thumb-drive mode and disables keyboard input.
2. **`JP2` / `JP3` (Power Source Selector)**:
   * **Set to `USB`** (power drawn from micro-USB cable `J4`).
3. **Buttons**:
   * **`btnC` (Center push button `U18`):** Soft CPU Reset (restarts the NEORV32 bootloader without wiping FPGA SRAM).
   * **`PROG` (Red button near top-left):** Reconfigures the FPGA SRAM from flash.

---

## Technical Documentation Suite

* **[`docs/DOOM_ENGINE_ARCHITECTURE.md`](docs/DOOM_ENGINE_ARCHITECTURE.md)**: Raycasting DDA math, texture mapping algorithms, 1D Z-Buffer occlusion, and Id Software BSP tree traversal.
* **[`docs/HARDWARE_SPECIFICATION.md`](docs/HARDWARE_SPECIFICATION.md)**: Hardware register maps, 8x8 systolic coprocessor timings, and VGA scanout timing diagrams.
* **[`docs/DOOM_GLITCH_ANALYSIS.md`](docs/DOOM_GLITCH_ANALYSIS.md)**: Deep dive into the 19.2 KB size wall, write-only VRAM read-modify-write trap, and Hall-of-Mirrors glitch resolution.
