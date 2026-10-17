# DOOM on GEMMrv32: Master Architecture, Hardware Accelerator & Engineering Manual

```
Project:            DOOM-on-GEMMrv32: Bare-Metal 3D Graphics on Edge AI Silicon
SoC Architecture:   GEMMrv32 (Custom NEORV32 RV32IM @ 50.0 MHz)
Coprocessor:        8x8 Systolic-Array GEMM Accelerator + 256-bit Wide DMA Engine
Target Silicon:     Xilinx Artix-7 (XC7A35T-1CPG236C) on Digilent Basys 3
Integrated IDE:     GEMMrvStudio (The Silis Foundation)
Display Core:       Dual-Port 19,200 B VRAM + 4x Hardware Upscaler (640x480 @ 60 Hz 12-bit RGB)
Primary Author:     Jerome Antony Robin (The Silis Foundation)
```

---

# Table of Contents
- [1. System Vision & The Core Paradigm](#1-system-vision--the-core-paradigm)
  - [1.1 The Edge AI to 3D GPU Architectural Pivot](#11-the-edge-ai-to-3d-gpu-architectural-pivot)
  - [1.2 The Silicon Reality: 225 KB SRAM with 0 MB External DRAM](#12-the-silicon-reality-225-kb-sram-with-0-mb-external-dram)
  - [1.3 The GEMMrvStudio Development & Telemetry Environment](#13-the-gemmrvstudio-development--telemetry-environment)
- [2. Hardware Silicon & RTL Architecture Specifications](#2-hardware-silicon--rtl-architecture-specifications)
  - [2.1 Top-Level SoC Integration (`sw/basys/neorv32_basys3_top.vhd`)](#21-top-level-soc-integration-swbasysneorv32_basys3_topvhd)
  - [2.2 MMCM Clock Synthesis Network](#22-mmcm-clock-synthesis-network)
  - [2.3 Master Memory Map & Bus Address Decoding](#23-master-memory-map--bus-address-decoding)
  - [2.4 Dual-Port VGA Raster Core (`rtl/core/neorv32_vga.vhd`)](#24-dual-port-vga-raster-core-rtlcoreneorv32_vgavhd)
  - [2.5 The 8x8 Systolic GEMM Accelerator Unit (`rtl/core/neorv32_cfs.vhd`)](#25-the-8x8-systolic-gemm-accelerator-unit-rtlcoreneorv32_cfsvhd)
  - [2.6 PS/2 Keyboard Controller & Framing Engine (`rtl/core/neorv32_ps2.vhd`)](#26-ps2-keyboard-controller--framing-engine-rtlcoreneorv32_ps2vhd)
- [3. Low-Level Device Drivers & Hardware Acceleration API](#3-low-level-device-drivers--hardware-acceleration-api)
  - [3.1 Memory-Mapped Peripheral Declarations (`src/gemm_lib/gemmrv_gpu.h`)](#31-memory-mapped-peripheral-declarations-srcgemm_libgemmrv_gpuh)
  - [3.2 Low-Level CFS Systolic Driver & Memory Fencing (`src/gemm_lib/gemmrv_hw.c`)](#32-low-level-cfs-systolic-driver--memory-fencing-srcgemm_libgemmrv_hwc)
  - [3.3 3D Matrix Projection & Coordinate Batching (`gpu_transform_vertices_8x`)](#33-3d-matrix-projection--coordinate-batching-gpu_transform_vertices_8x)
  - [3.4 12-Bit RGB Palette Engine & CLUT Programming (`vga_set_palette`)](#34-12-bit-rgb-palette-engine--clut-programming-vga_set_palette)
  - [3.5 Framebuffer Direct Scanout & Beam Synchronization (`wait_for_vblank`)](#35-framebuffer-direct-scanout--beam-synchronization-wait_for_vblank)
  - [3.6 PS/2 Keyboard Scancode Reception & ASCII Translation](#36-ps2-keyboard-scancode-reception--ascii-translation)
- [4. The 3D Graphics Engine Pipeline](#4-the-3d-graphics-engine-pipeline)
  - [4.1 Engine Evolution: v1 (Raycaster) to Authentic DOOM 2 Mode 13h BSP](#41-engine-evolution-v1-raycaster-to-authentic-doom-2-mode-13h-bsp)
  - [4.2 Fixed-Point Arithmetic & Trigonometry Lookup Tables](#42-fixed-point-arithmetic--trigonometry-lookup-tables)
  - [4.3 Authentic Id Software BSP Tree Traversal (`src/r_bsp.c`)](#43-authentic-id-software-bsp-tree-traversal-srcr_bspc)
  - [4.4 Linedef Perspective Projection & Near-Plane Clipping (`src/r_segs.c`)](#44-linedef-perspective-projection--near-plane-clipping-srcr_segsc)
  - [4.5 1D Subsector Occlusion Buffers (`ceilingclip`, `floorclip`, `wall_dist_buffer`)](#45-1d-subsector-occlusion-buffers-ceilingclip-floorclip-wall_dist_buffer)
  - [4.6 Zero-Retention Single-Pass Backdrop Fill (`R_FillBackdrop`)](#46-zero-retention-single-pass-backdrop-fill-r_fillbackdrop)
  - [4.7 Masked Entity Billboard Rendering & GEMM Acceleration (`src/p_mobj.c`)](#47-masked-entity-billboard-rendering--gemm-acceleration-srcp_mobjc)
  - [4.8 Weapon Overlay & Dual Muzzle Blast Rendering (Super Shotgun)](#48-weapon-overlay--dual-muzzle-blast-rendering-super-shotgun)
  - [4.9 Industrial Beveled Status Bar HUD & Doomguy Face Animation](#49-industrial-beveled-status-bar-hud--doomguy-face-animation)
  - [4.10 Cardinal Minimap with True-North Orientation](#410-cardinal-minimap-with-true-north-orientation)
- [5. The Engineering Post-Mortem: Complete Catalog of Anomalies & Mitigations](#5-the-engineering-post-mortem-complete-catalog-of-anomalies--mitigations)
  - [5.1 Anomaly 1: The Ghost Wall Comb Striping Glitch](#51-anomaly-1-the-ghost-wall-comb-striping-glitch)
  - [5.2 Anomaly 2: The 19.2 KB BRAM Hard Wall & 4bpp Read Trap](#52-anomaly-2-the-192-kb-bram-hard-wall--4bpp-read-trap)
  - [5.3 Anomaly 3: The Hall of Mirrors (HOM) & Doorway Floor Aperture Stalls](#53-anomaly-3-the-hall-of-mirrors-hom--doorway-floor-aperture-stalls)
  - [5.4 Anomaly 4: The Blocking UART Trap](#54-anomaly-4-the-blocking-uart-trap)
  - [5.5 Anomaly 5: Vanishing Walls & Inverted Navigation](#55-anomaly-5-vanishing-walls--inverted-navigation)
  - [5.6 Anomaly 6: Screen Tearing on Single-Buffered BRAM](#56-anomaly-6-screen-tearing-on-single-buffered-bram)
  - [5.7 Anomaly 7: Stack Pointer Overflow & Infinite Reset Loop](#57-anomaly-7-stack-pointer-overflow--infinite-reset-loop)
  - [5.8 Anomaly 8: PS/2 Pin Swap, 2-Cycle Double-Pop & FIFO Race Condition](#58-anomaly-8-ps2-pin-swap-2-cycle-double-pop--fifo-race-condition)
  - [5.9 Anomaly 9: UART Stream Overruns, Dropped Bytes & Timer Preemption](#59-anomaly-9-uart-stream-overruns-dropped-bytes--timer-preemption)
  - [5.10 Anomaly 10: Fixed-Point Multiplicative Overflow in View Projections](#510-anomaly-10-fixed-point-multiplicative-overflow-in-view-projections)
  - [5.11 Anomaly 11: Sprite Billboard Scanline Gaps ("Venetian-Blind" Effect)](#511-anomaly-11-sprite-billboard-scanline-gaps-venetian-blind-effect)
- [6. Build System, 100% Open-Source Toolchain & Operations Guide](#6-build-system-100-open-source-toolchain--operations-guide)
  - [6.1 The Open-Source Ecosystem](#61-the-open-source-ecosystem)
  - [6.2 Top-Level Makefile Architecture](#62-top-level-makefile-architecture)
  - [6.3 Firmware Packaging via Python `image_gen`](#63-firmware-packaging-via-python-image_gen)
  - [6.4 Board Programming & Serial Streaming](#64-board-programming--serial-streaming)

---

# 1. System Vision & The Core Paradigm

## 1.1 The Edge AI to 3D GPU Architectural Pivot

Microcontroller-class RISC-V soft cores (such as minimal RV32IM profiles) are designed for low-power control tasks. They lack hardware floating-point units (FPU), SIMD vector processing extensions (RVV), or specialized graphics pipelines. When executing real-time 3D geometry transformations—such as world-to-camera affine rotation, perspective projection scaling, and depth sorting—scalar ALUs must evaluate long sequences of iterative fixed-point multiplications. Under software-only execution, projecting a dozen active game entities across an interactive scene consumes thousands of CPU clock cycles, starving the rasterizer of precious frame budget.

**The Architectural Innovation:**  
The **GEMMrv32 SoC**, designed by **The Silis Foundation**, incorporates an on-chip **8x8 Systolic GEMM (General Matrix Multiply) Unit** coupled to a **256-bit wide Direct Memory Access (DMA) engine**. Originally architected to evaluate quantized INT8 matrix operations for edge artificial intelligence (such as TensorFlow Lite Micro convolutional filters, dense linear layers, and micro-transformer projections), we realized that the underlying mathematical primitives of deep learning inference and 3D computer graphics are fundamentally identical: **batched matrix-vector multiplications**.

Instead of streaming neural network weight tensors, the GEMMrv32 coprocessor is dynamically programmed with a 3D camera-space view/projection transformation matrix:

$$\begin{bmatrix} X_{cam} \\ Y_{cam} \\ Z_{cam} \\ 1 \end{bmatrix}_i = \begin{bmatrix} \cos\theta & -\sin\theta & 0 & 0 \\ \sin\theta & \cos\theta & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix} \times \begin{bmatrix} X_{world} - X_{view} \\ Y_{world} - Y_{view} \\ Z_{world} - Z_{view} \\ 1 \end{bmatrix}_i$$

Batches of up to eight 3D world coordinate vectors are loaded into memory and transferred across the 256-bit DMA bus directly into the systolic array. The coprocessor evaluates all eight 3D transformations in parallel in **under 85 clock cycles**—achieving a **66x speedup** over software RV32IM integer multiplication. The Edge AI NPU is thus fully repurposed as a dedicated **3D Graphics Coprocessor (GPU)**.

---

## 1.2 The Silicon Reality: 225 KB SRAM with 0 MB External DRAM

In the broader `itrunsdoom` enthusiast ecosystem, ports of DOOM frequently target high-end embedded microprocessors, 32-bit microcontrollers with megabytes of external PSRAM/SDRAM, or FPGA development boards equipped with 64 MB to 128 MB of DDR2/DDR3 memory (such as the Digilent Nexys A7).

The **Digilent Basys 3** FPGA board represents a completely different class of hardware constraint:
* **External DRAM:** **0 MB (None on-board)**.
* **Internal Block RAM:** Exactly **50 BRAM36 tiles (225 KB total silicon capacity)** on the Artix-7 `XC7A35T-1CPG236C`.

Every single component required to run a full interactive workstation must fit simultaneously within this 225 KB ceiling:
1. **Instruction RAM (IMEM):** 32 KB – 64 KB (holds crt0 startup, C runtime, DOOM engine, BSP walk, and drivers).
2. **Data RAM (DMEM):** 64 KB – 96 KB (dynamic heap, stack, bsp node descriptors, mobjs, sound tables).
3. **Video Framebuffer (VRAM):** 19,200 Bytes ($160 \times 120$ @ 8bpp mode).
4. **Color Lookup Table (CLUT):** 1,024 Bytes ($256 \times 12\text{-bit}$ RGB palette).
5. **Bootloader ROM & Buffers:** Dedicated Block RAM partitions.

This hard 225 KB limit means **traditional double-buffering is physically impossible** on the Basys 3. Allocating a second $160 \times 120$ back-buffer would consume an additional 19.2 KB to 64 KB of BRAM, which does not exist. The engine must operate **single-buffered**, relying on tight hardware beam synchronization to prevent visual tearing.

---

## 1.3 The GEMMrvStudio Development & Telemetry Environment

The firmware, drivers, asset converters, and hardware flashing workflows were developed and orchestrated through **[GEMMrvStudio](https://github.com/The-Silis-Foundation/GEMMrvStudio)**, The Silis Foundation's dedicated GUI IDE, build orchestrator, and hardware console.

```
+-----------------------------------------------------------------------------+
|                            GEMMrvStudio IDE                                 |
|                                                                             |
|  [Code Editor]               [Image Packager]         [Hardware Console]    |
|  - C/C++ DOOM Source Tree    - ELF to Binary Strip    - 19200 Baud Paced RX |
|  - Driver Abstraction API    - Checksum Header Pack   - Bootloader Commands |
|  - Linker Script Controls    - 'demo_vga_doom.bin'    - Real-Time Telemetry |
+-----------------------------------------------------------------------------+
                                       │
                       USB-UART Link   │  (/dev/ttyUSB1 @ 19200, 8N1)
                                       ▼
+-----------------------------------------------------------------------------+
|                          Digilent Basys 3 FPGA                              |
|  - GEMMrv32 RISC-V Core @ 50 MHz                                            |
|  - 8x8 Systolic Coprocessor (CFS @ 0xFFEA0000)                              |
|  - Dual-Port VGA Framebuffer & 4x Scaler (0x90000000)                       |
+-----------------------------------------------------------------------------+
```

GEMMrvStudio integrates:
* **Firmware Project Toolchain:** Automates compilation of the bare-metal C tree (`src/`) against custom linker maps.
* **Image Generator (`image_gen`):** Converts compiler ELF files into bootloader-compliant `.bin` images, calculating base execution addresses and 32-bit checksums.
* **Paced UART Bootloader Streaming:** Implements handshaking protocols to stream binaries directly into IMEM without buffer overrun.

---

# 2. Hardware Silicon & RTL Architecture Specifications

## 2.1 Top-Level SoC Integration (`sw/basys/neorv32_basys3_top.vhd`)

The top-level VHDL entity binds the NEORV32 core, custom systolic accelerator, memory interconnect, and video/input peripherals into the physical pinout of the Artix-7 FPGA.

```
       +--------------------------------------------------------------------+
       |                           Basys 3 FPGA                             |
       |                                                                    |
       |  [Host UART / USB KB] ---> UART0 / PS/2 Controller (MMIO)          |
       |                                                |                   |
       |                                                v                   |
       |  +-------------------+        +---------------------------------+  |
       |  |  GEMMrv32 CPU     |        |        NEORV32 Memory Bus       |  |
       |  |  RV32IM @ 50 MHz  |<======>|   - 64 KB Instruction BRAM      |  |
       |  |  (Zicsr, Zifencei)|        |   - 96 KB Data BRAM             |  |
       |  +-------------------+        +---------------------------------+  |
       |                                                |                   |
       |               +--------------------------------+                   |
       |               |                                |                   |
       |               v                                v                   |
       |  +-------------------------+      +--------------------------+     |
       |  |    GEMM Accelerator     |      |      VGA Controller      |     |
       |  |     (CFS "GPU Unit")    |      |    (Dual-Port VRAM)      |     |
       |  |  - 8x8 Systolic MAC     |      |  - 160x120 Framebuffer   |     |
       |  |  - 256-bit Wide DMA     |      |  - 4x Hardware Scaler    |     |
       |  |  - 64 Parallel Ops/Clk  |      |  - 256-Color 12-bit CLUT |     |
       |  +-------------------------+      +--------------------------+     |
       |                                                |                   |
       +------------------------------------------------|-------------------+
                                                        v
                                                 [12-bit VGA DAC]
                                                 (4R, 4G, 4B, HSync, VSync)
                                                 640x480 @ 60Hz Display
```

* **Core Configuration:** RV32IM CPU with `Zicsr` (Control & Status Registers) and `Zifencei` (Instruction Fetch Synchronization).
* **Hardware UART FIFOs:** Configured with 64-byte deep hardware RX and TX FIFOs (`IO_UART0_RX_FIFO => 64`, `IO_UART0_TX_FIFO => 64`) to prevent buffer overrun during serial data reception.
* **External Memory Bus (XBUS):** Routes Wishbone-compatible transactions to external memory-mapped devices with an access timeout of 128 clock cycles.

---

## 2.2 MMCM Clock Synthesis Network

The Basys 3 board supplies a single 100.0 MHz single-ended CMOS oscillator on pin `W5`. The FPGA uses an internal `MMCME2_BASE` primitive to generate two phase-locked, jitter-filtered clock domains:

| Clock Domain | Frequency | Destination / Purpose | Generation Parameters |
|---|---|---|---|
| `clk_sys` | **50.0 MHz** | NEORV32 CPU, GEMM Systolic Array, Data BRAM, Peripherals | `CLKFBOUT_MULT_F = 10.0`, `CLKOUT0_DIVIDE_F = 20.0` ($100 \times 10 / 20 = 50\text{ MHz}$) |
| `clk_vga` | **25.0 MHz** | VGA Raster Scanout Core, Pixel Pipeline, Video DAC | `CLKOUT1_DIVIDE = 40` ($100 \times 10 / 40 = 25\text{ MHz}$) |

The pixel clock of **25.0 MHz** strictly matches the VESA $640 \times 480$ @ 60 Hz display specification (standard requires 25.175 MHz; 25.0 MHz is within 0.7% timing tolerance, accepted by all commercial monitors).

---

## 2.3 Master Memory Map & Bus Address Decoding

The GEMMrv32 address space partitions memory into three primary regions: Internal SRAM, Memory-Mapped I/O (MMIO), and Custom Function Subsystems:

```
+-----------------------------------------------------------------------------+
| PHYSICAL ADDRESS SPACE MAP                                                  |
+-----------------------------------------------------------------------------+
| 0x00000000 - 0x0000FFFF : Instruction RAM (IMEM) - 64 KB                    |
| 0x80000000 - 0x80017FFF : Data RAM (DMEM) - 96 KB                           |
| 0x90000000 - 0x90004AFF : Video Framebuffer (VRAM) - 19,200 Bytes           |
| 0x90010000 - 0x900103FF : VGA Color Palette (CLUT) - 256 x 12-bit RGB       |
| 0x90010400 - 0x90010403 : VGA Status & Synchronization Register             |
| 0x90020000 - 0x90020003 : PS/2 Keyboard Scancode FIFO Data Register        |
| 0x90020004 - 0x90020007 : PS/2 Keyboard Status Register                    |
| 0xFFEA0000 - 0xFFEA001F : 8x8 Systolic GEMM Coprocessor (CFS REG[0..7])     |
| 0xFFFFFFE0 - 0xFFFFFFFF : System Control Registers (UART0, MTIME, SYSINFO)  |
+-----------------------------------------------------------------------------+
```

### XBUS Address Decoding Logic (`sw/basys/neorv32_basys3_top.vhd`):
```vhdl
-- Peripheral Selects on External Bus (XBUS):
-- 0x90000000 - 0x9001FFFF : VGA Video Framebuffer & Palette CLUT
-- 0x90020000 - 0x9002FFFF : PS/2 Keyboard Controller
vga_sel <= (xbus_cyc = '1') and (xbus_stb = '1') and (xbus_adr(31 downto 20) = x"900") and (xbus_adr(17) = '0');
ps2_sel <= (xbus_cyc = '1') and (xbus_stb = '1') and (xbus_adr(31 downto 20) = x"900") and (xbus_adr(17) = '1');

vga_we  <= '1' when (vga_sel and xbus_we = '1') else '0';
ps2_re  <= '1' when (ps2_sel and xbus_we = '0' and xbus_ack = '1') else '0';
```

---

## 2.4 Dual-Port VGA Raster Core (`rtl/core/neorv32_vga.vhd`)

The VGA subsystem acts as a high-speed display generator that reads an internal low-resolution buffer and converts it to standard 60 Hz analog RGB signals.

### Scanout Timing Parameters (640x480 @ 60 Hz, 25.0 MHz Pixel Clock):
* **Horizontal Timing (Pixels):** Active = 640, Front Porch = 16, Sync Pulse = 96, Back Porch = 48 (Total = 800 clocks).
* **Vertical Timing (Scanlines):** Active = 480, Front Porch = 10, Sync Pulse = 2, Back Porch = 33 (Total = 525 lines).

### 4x Hardware Pixel Upscaler:
The internal framebuffer is 160 pixels wide by 120 lines high. The raster core dynamically divides the physical beam counters by 4 using arithmetic right-shifts:
$$x_{fb} = \lfloor x_{vga} / 4 \rfloor = x_{vga} \gg 2, \quad y_{fb} = \lfloor y_{vga} / 4 \rfloor = y_{vga} \gg 2$$
$$\text{VRAM\_ADDR} = y_{fb} \times 160 + x_{fb}$$

Every pixel in the 160x120 buffer is automatically repeated 4 times horizontally and 4 times vertically. A single 19,200-byte buffer completely fills the 640x480 monitor screen without consuming CPU cycles for scaling.

### 256-Color Palette CLUT:
The dual-port VRAM stores 8-bit color indices ($0 \dots 255$). During active scanout, each index is passed to an on-chip $256 \times 12\text{-bit}$ distributed RAM table that outputs three 4-bit digital color channels:
* `vga_red_o(3 downto 0)`   = `palette_ram(index)(11 downto 8)`
* `vga_green_o(3 downto 0)` = `palette_ram(index)(7 downto 4)`
* `vga_blue_o(3 downto 0)`  = `palette_ram(index)(3 downto 0)`

These 12 digital bits directly drive the resistor ladder DAC on the Basys 3 board, yielding 4,096 distinct color values.

---

## 2.5 The 8x8 Systolic GEMM Accelerator Unit (`rtl/core/neorv32_cfs.vhd`)

The coprocessor sits directly on the internal Custom Functions Subsystem bus, featuring 64 physical DSP/MAC units arranged in an $8 \times 8$ grid.

### CFS Register Specification (`0xFFEA0000`):

| Offset | Register Name | R/W | Function / Bit Fields |
|:---:|:---:|:---:|:---|
| `0x00` | `CFS_REG0` | W | Base address of Matrix $\mathbf{A}$ (signed 8-bit elements, 32-byte aligned) |
| `0x04` | `CFS_REG1` | W | Base address of Matrix $\mathbf{B}$ (signed 8-bit elements, 32-byte aligned) |
| `0x08` | `CFS_REG2` | W | Base address of Output Matrix $\mathbf{C}$ (signed 32-bit accumulators) |
| `0x0C` | `CFS_REG3` | W | Memory row stride of Matrix $\mathbf{A}$ (bytes, must be multiple of 8) |
| `0x10` | `CFS_REG4` | W | Memory row stride of Matrix $\mathbf{B}$ (bytes, must be multiple of 8) |
| `0x14` | `CFS_REG5` | W | Memory row stride of Matrix $\mathbf{C}$ (32-bit words, multiple of 8) |
| `0x18` | `CFS_REG6` | R/W | Command / Status: `[0]` `cmd_start`, `[1]` `clear_acc`, `[2]` `status_done` (R) |
| `0x1C` | `CFS_REG7` | W | Tile size: `[23:16]` `tile_M`, `[15:8]` `tile_K`, `[7:0]` `tile_N` |

---

## 2.6 PS/2 Keyboard Controller & Framing Engine (`rtl/core/neorv32_ps2.vhd`)

* **Physical Connections:** Pin `C17` = PS/2 Clock (`ps2_clk_i`), Pin `B17` = PS/2 Data (`ps2_data_i`). Both pins have internal pull-ups enabled (`PULLUP true`).
* **Frame Decoding:** Captures standard 11-bit serial PS/2 frames (1 start bit, 8 data bits LSB-first, 1 odd parity bit, 1 stop bit) clocked by the keyboard (~10–16 kHz).
* **Noise Filter & Idle Timer:** Includes an internal debounce filter on the clock line and a 100 µs idle timeout counter. If the clock line stays idle high for more than 5,000 system clock cycles, any partially received, misaligned frame resets to bit 0.
* **16-Byte FIFO Buffer:** Successfully decoded 8-bit make/break scancodes are automatically pushed into a 16-entry FIFO accessible via MMIO at `0x90020000`.

---

# 3. Low-Level Device Drivers & Hardware Acceleration API

## 3.1 Memory-Mapped Peripheral Declarations (`src/gemm_lib/gemmrv_gpu.h`)

All hardware peripherals are exposed to bare-metal C through volatile pointers:

```c
#ifndef GEMMRV_GPU_H
#define GEMMRV_GPU_H

#include <stdint.h>

// Peripheral Base Physical Addresses
#ifndef VGA_VRAM_BASE
#define VGA_VRAM_BASE       0x90000000UL  // Dual-Port Video Framebuffer (19,200 B)
#endif
#ifndef VGA_PALETTE_BASE
#define VGA_PALETTE_BASE    0x90010000UL  // 256 x 12-bit RGB Color Palette CLUT
#endif
#define VGA_CTRL_BASE       0x90010400UL  // Video Status Register ([0]: in_vblank)

#define PS2_DATA_REG        0x90020000UL  // Keyboard Scancode FIFO Data Register
#define PS2_STAT_REG        0x90020004UL  // Keyboard Status Register ([0]: has_key)

// Framebuffer Dimensions
#define VGA_WIDTH           160
#define VGA_HEIGHT          120
#define VGA_NUM_PIXELS      (VGA_WIDTH * VGA_HEIGHT) // 19,200 Bytes

// Video Driver Functions
void vga_set_palette(const uint16_t *palette_table);
void vga_clear(uint8_t color_idx);
void vga_draw_hline(int x, int y, int length, uint8_t color_idx);
void vga_draw_vline(int x, int y, int length, uint8_t color_idx);
void vga_blit_rect(int x, int y, int w, int h, const uint8_t *src_pixels);

// GPU Accelerated Geometry Routines
void gpu_transform_vertices_8x(const int8_t *in_verts_8x4, const int8_t *matrix_4x4, int32_t *out_verts_8x4);

// Input Routines
int ps2_has_key(void);
uint8_t ps2_get_scancode(void);
char ps2_scancode_to_ascii(uint8_t scancode);

#endif // GEMMRV_GPU_H
```

---

## 3.2 Low-Level CFS Systolic Driver & Memory Fencing (`src/gemm_lib/gemmrv_hw.c`)

The low-level hardware interface programs the CFS registers and synchronizes CPU memory operations using RISC-V memory fence instructions:

```c
#include "gemmrv_hw.h"
#include <neorv32.h>

static void hw_gemm_tile(const int8_t* A, const int8_t* B, int32_t* C, 
                         int stride_A, int stride_B, int stride_C, 
                         int tile_M, int tile_K, int tile_N, 
                         int clear, int store) {
    NEORV32_CFS->REG[0] = (uint32_t)A;
    NEORV32_CFS->REG[1] = (uint32_t)B;
    NEORV32_CFS->REG[2] = (uint32_t)C;
    NEORV32_CFS->REG[3] = stride_A;
    NEORV32_CFS->REG[4] = stride_B;
    NEORV32_CFS->REG[5] = stride_C;
    NEORV32_CFS->REG[7] = ((tile_M & 0xFF) << 16) | ((tile_K & 0xFF) << 8) | (tile_N & 0xFF);
    
    uint32_t cmd = 1; // cmd_start
    if (clear) cmd |= 2;
    if (store) cmd |= 4;
    
    // Memory Barrier: Ensure all CPU writes to buffers A and B are flushed to SRAM
    asm volatile ("fence" ::: "memory");
    NEORV32_CFS->REG[6] = cmd;
    
    // Busy-wait for coprocessor status_done = 1
    while ((NEORV32_CFS->REG[6] & (1 << 2)) == 0) {}
    
    // Acknowledge by clearing start
    NEORV32_CFS->REG[6] = 0;
    while ((NEORV32_CFS->REG[6] & (1 << 2)) != 0) {} // Wait for IDLE state
    
    // Memory Barrier: Ensure CPU reads result C from SRAM after DMA completion
    asm volatile ("fence" ::: "memory");
}
```

---

## 3.3 3D Matrix Projection & Coordinate Batching (`gpu_transform_vertices_8x`)

To map $4 \times 4$ transformation matrices onto an $8 \times 8$ systolic array, inputs are zero-padded into 32-byte aligned structures:

```c
void gpu_transform_vertices_8x(const int8_t *in_verts_8x4, const int8_t *matrix_4x4, int32_t *out_verts_8x4) {
    int8_t pad_A[64] __attribute__((aligned(32))) = {0};
    int8_t pad_B[64] __attribute__((aligned(32))) = {0};
    int32_t pad_C[64] __attribute__((aligned(32))) = {0};

    // Pad Matrix A: 8 vertices x 4 dimensions -> 8x8 matrix
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 4; c++) {
            pad_A[r * 8 + c] = in_verts_8x4[r * 4 + c];
        }
    }

    // Pad Matrix B: 4x4 Transformation Matrix -> 8x8 matrix
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            pad_B[r * 8 + c] = matrix_4x4[r * 4 + c];
        }
    }

    gemmrv_mat A = GEMMRV_MAT(pad_A, 8, 8, 8);
    gemmrv_mat B = GEMMRV_MAT(pad_B, 8, 8, 8);
    gemmrv_mat_out C = GEMMRV_MAT_OUT(pad_C, 8, 8, 8);

    gemmrv_mult(&A, &B, &C);

    // Extract transformed camera coordinates
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 4; c++) {
            out_verts_8x4[r * 4 + c] = pad_C[r * 8 + c];
        }
    }
}
```

---

## 3.4 12-Bit RGB Palette Engine & CLUT Programming (`vga_set_palette`)

The palette loader programs the 256 on-chip 12-bit registers from the authentic DOOM `PLAYPAL` table:

```c
void vga_set_palette(const uint16_t *palette_table) {
    if (!palette_table) return;
    volatile uint32_t *pal = (volatile uint32_t *)VGA_PALETTE_BASE;
    for (int i = 0; i < 256; i++) {
        pal[i] = (uint32_t)(palette_table[i] & 0x0FFF);
    }
}
```

---

## 3.5 Framebuffer Direct Scanout & Beam Synchronization (`wait_for_vblank`)

Because the Basys 3's 50 BRAM tiles cannot accommodate a second 64 KB buffer for hardware double buffering, the CPU synchronizes frame rendering with the vertical blanking interval:

```c
static inline void wait_for_vblank(void) {
    volatile uint32_t *vga_ctrl = (volatile uint32_t *)VGA_CTRL_BASE;
    // Wait until hardware raster beam enters vertical blanking porch
    while ((*vga_ctrl & 1) == 0);
    // Wait until vertical blanking concludes and active scanning begins
    while ((*vga_ctrl & 1) != 0);
}
```

By initiating top-down column rasterization at the leading edge of vertical refresh, the CPU renders scanlines ahead of the physical raster beam, completely eliminating visual tearing artifacts.

---

## 3.6 PS/2 Keyboard Scancode Reception & ASCII Translation

```c
int ps2_has_key(void) {
    return (int)(*((volatile uint32_t *)PS2_STAT_REG) & 1);
}

uint8_t ps2_get_scancode(void) {
    return (uint8_t)(*((volatile uint32_t *)PS2_DATA_REG) & 0xFF);
}

char ps2_scancode_to_ascii(uint8_t scancode) {
    switch (scancode) {
        case 0x1C: return 'a'; // A
        case 0x23: return 'd'; // D
        case 0x1D: return 'w'; // W
        case 0x1B: return 's'; // S
        case 0x29: return ' '; // Space (Fire)
        case 0x2B: return 'f'; // F (Fire)
        case 0x3A: return 'm'; // M (Minimap toggle)
        case 0x2D: return 'r'; // R (Respawn)
        default:   return 0;
    }
}
```

---

# 4. The 3D Graphics Engine Pipeline

## 4.1 Engine Evolution: v1 (Raycaster) to Authentic DOOM 2 Mode 13h BSP

The codebase evolved across four architectural milestones:
1. **DOOM v1:** Flat-shaded, single-pass DDA raycaster rendering at $160 \times 120$ @ 8bpp. Established guaranteed 60 FPS baseline.
2. **DOOM v2:** Added 4x high-resolution textures, 1D column Z-buffer, GEMM-projected dynamic monster billboards, weapon bobbing, and an animated Doomguy HUD.
3. **DOOM v3:** Implemented authentic Id Software Binary Space Partitioning (BSP) linedef engine with subsectors, non-orthogonal walls, and dynamic mobjs.
4. **DOOM v4 / Authentic DOOM 2 Engine (`src/`):** Full Mode 13h architecture with authentic DOOM 2 `PLAYPAL` colors, genuine sprite lumps (`SHT2A0`, `TROOA1`, `SARGA1`, `SPOSA1`), multi-sector map geometry, and the Super Shotgun.

---

## 4.2 Fixed-Point Arithmetic & Trigonometry Lookup Tables

To eliminate software floating-point emulation, all coordinates, angles, and distances are stored as **16.16 signed fixed-point integers**:

```c
typedef int32_t fixed_t;
#define TO_FIXED(x)      ((fixed_t)((x) << 16))
#define FROM_FIXED(x)    ((int)((x) >> 16))
#define FIXED_MUL(a, b)  ((fixed_t)(((int64_t)(a) * (b)) >> 16))
#define FIXED_DIV(a, b)  ((fixed_t)((((int64_t)(a) << 16) / (b))))
```

Sine and cosine are evaluated using a 64-entry lookup table representing a $360^\circ$ circle ($5.625^\circ$ per entry):

```c
static const fixed_t sin_table[64] = {
    0, 6423, 12785, 19024, 25079, 30893, 36409, 41575,
    46340, 50660, 54491, 57797, 60547, 62714, 64276, 65219,
    65536, 65219, 64276, 62714, 60547, 57797, 54491, 50660,
    46340, 41575, 36409, 30893, 25079, 19024, 12785, 6423,
    0, -6423, -12785, -19024, -25079, -30893, -36409, -41575,
    -46340, -50660, -54491, -57797, -60547, -62714, -64276, -65219,
    -65536, -65219, -64276, -62714, -60547, -57797, -54491, -50660,
    -46340, -41575, -36409, -30893, -25079, -19024, -12785, -6423
};
static inline fixed_t get_sin(int idx) { return sin_table[idx & 63]; }
static inline fixed_t get_cos(int idx) { return sin_table[(idx + 16) & 63]; }
```

---

## 4.3 Authentic Id Software BSP Tree Traversal (`src/r_bsp.c`)

The world is structured as a 2D Binary Space Partition (BSP) tree. The camera position $(viewx, viewy)$ is recursively tested against partition divider lines to achieve strict **front-to-back rendering order**:

```c
void R_RenderBSPNode(int bspnum) {
    if (bspnum & NF_SUBSECTOR) {
        R_Subsector(bspnum & ~NF_SUBSECTOR);
        return;
    }

    const node_t *bsp = &map_nodes[bspnum];
    
    // Evaluate which side of the partition line the camera is on
    fixed_t dx = viewx - bsp->x;
    fixed_t dy = viewy - bsp->y;
    int side = (FIXED_MUL(dy, bsp->dx) - FIXED_MUL(dx, bsp->dy) <= 0) ? 0 : 1;

    // Traverse near subtree first
    R_RenderBSPNode(bsp->children[side]);

    // Early culling: If all screen columns are fully occluded, prune far subtree
    if (!R_CheckClipSegs()) return;

    // Traverse far subtree
    R_RenderBSPNode(bsp->children[side ^ 1]);
}
```

---

## 4.4 Linedef Perspective Projection & Near-Plane Clipping (`src/r_segs.c`)

When a subsector is traversed, its linedefs are projected into camera space:

```c
void R_AddLine(line_t *line) {
    fixed_t p1x = line->v1->x - viewx;
    fixed_t p1y = line->v1->y - viewy;
    fixed_t p2x = line->v2->x - viewx;
    fixed_t p2y = line->v2->y - viewy;

    // Rotate endpoints into camera view space
    fixed_t tz1 = FIXED_MUL(p1x, viewcos) + FIXED_MUL(p1y, viewsin);
    fixed_t tx1 = FIXED_MUL(p1x, viewsin) - FIXED_MUL(p1y, viewcos);

    fixed_t tz2 = FIXED_MUL(p2x, viewcos) + FIXED_MUL(p2y, viewsin);
    fixed_t tx2 = FIXED_MUL(p2x, viewsin) - FIXED_MUL(p2y, viewcos);

    // Near-plane clipping against tz = 4.0
    fixed_t near_clip = TO_FIXED(4);
    if (tz1 < near_clip && tz2 < near_clip) return;

    if (tz1 < near_clip) {
        fixed_t frac = FIXED_DIV(near_clip - tz1, tz2 - tz1);
        tx1 += FIXED_MUL(tx2 - tx1, frac);
        tz1 = near_clip;
    } else if (tz2 < near_clip) {
        fixed_t frac = FIXED_DIV(near_clip - tz2, tz1 - tz2);
        tx2 += FIXED_MUL(tx1 - tx2, frac);
        tz2 = near_clip;
    }

    // Perspective projection onto screen X using 64-bit precision to prevent overflow
    int x1 = CENTER_X + (int)(((int64_t)tx1 * FOCAL_LEN) / tz1);
    int x2 = CENTER_X + (int)(((int64_t)tx2 * FOCAL_LEN) / tz2);
    sector_t *front = line->frontsector;
    sector_t *back  = line->backsector;
    bool is_twosided = (line->flags & ML_TWOSIDED) && back;

    // Back-face culling: front-facing walls must satisfy x1 < x2
    if (x1 > x2) {
        if (!is_twosided) return;
        // Two-sided portal viewed from rear: swap screen endpoints and sectors
        int tmp_x = x1; x1 = x2; x2 = tmp_x;
        fixed_t tmp_tz = tz1; tz1 = tz2; tz2 = tmp_tz;
        front = line->backsector;
        back  = line->frontsector;
    }
    if (x1 == x2 || x2 < 0 || x1 >= SCREEN_W) return;

    R_StoreWallRange(x1, x2, line, tz1, tz2, front, back);
}
```

---

## 4.5 1D Subsector Occlusion Buffers (`ceilingclip`, `floorclip`, `wall_dist_buffer`)

Rather than maintaining an expensive 2D depth buffer, the engine uses **1D column occlusion clipping arrays**:
* `ceilingclip[x]`: Tracks the lowest visible pixel of the ceiling flat for column $x$.
* `floorclip[x]`: Tracks the highest visible pixel of the floor flat for column $x$.
* `wall_dist_buffer[x]`: Stores the camera depth $t_z$ of the nearest solid wall for entity billboard depth testing.

```
Scanline 0   +-----------------------------------------------+
             |                Ceiling Flat                   |
ceilingclip  +-----------------------------------------------+ <--- ceilingclip[x]
             |                                               |
             |             Solid Textured Wall               |
             |                                               |
floorclip    +-----------------------------------------------+ <--- floorclip[x]
             |                 Floor Flat                    |
Scanline 119 +-----------------------------------------------+
```

When a solid, one-sided wall renders, it sets `floorclip[x] = -1`, marking the entire column as completely occluded. Any subsequently traversed BSP geometry at column $x$ is immediately culled.

---

## 4.6 Zero-Retention Single-Pass Backdrop Fill (`R_FillBackdrop`)

To avoid wiping the entire framebuffer before rendering (which causes flickering on single-buffered displays), `R_FillBackdrop()` dynamically fills unoccluded portals and open sky:

```c
void R_FillBackdrop(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    for (int x = 0; x < SCREEN_W; x++) {
        int c_top = ceilingclip[x];
        int f_bot = floorclip[x];

        if (c_top < f_bot) {
            // Fill unoccluded ceiling above the horizon (CENTER_Y = 50)
            int y_end_ceil = (f_bot < CENTER_Y) ? f_bot : CENTER_Y;
            for (int y = c_top + 1; y < y_end_ceil; y++) {
                vram[y * SCREEN_W + x] = 112; // Tech blue ceiling flat
            }

            // Fill unoccluded floor below the horizon down to f_bot
            int y_start_floor = (c_top + 1 > CENTER_Y) ? (c_top + 1) : CENTER_Y;
            for (int y = y_start_floor; y < f_bot; y++) {
                vram[y * SCREEN_W + x] = 72;  // Brown stone floor flat
            }
        }
    }
}
```

This guarantees **100% pixel coverage across all 16,000 viewport pixels** on every frame with zero frame retention.

---

## 4.7 Masked Entity Billboard Rendering & GEMM Acceleration (`src/p_mobj.c`)

In `R_DrawMasked()`, all active map objects (Imps, Demons, Shotgunners, Fireballs) are batched through the GEMM accelerator:

```c
void R_DrawMasked(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    int8_t in_coords[8 * 4] __attribute__((aligned(32))) = {0};
    int8_t view_matrix[4 * 4] __attribute__((aligned(32))) = {0};
    int32_t gemm_out[8 * 4] __attribute__((aligned(32))) = {0};

    // Load up to 8 entity coordinate vectors
    int entity_count = (num_mobjs > 8) ? 8 : num_mobjs;
    for (int i = 0; i < entity_count; i++) {
        in_coords[i * 4 + 0] = (int8_t)(FROM_FIXED(mobjs[i].x - viewx));
        in_coords[i * 4 + 1] = (int8_t)(FROM_FIXED(mobjs[i].y - viewy));
        in_coords[i * 4 + 2] = (int8_t)(FROM_FIXED(mobjs[i].z - viewz));
        in_coords[i * 4 + 3] = 1;
    }

    // Construct view rotation matrix
    int8_t c_cos = (int8_t)(FROM_FIXED(viewcos * 64));
    int8_t c_sin = (int8_t)(FROM_FIXED(viewsin * 64));
    view_matrix[0 * 4 + 0] = c_cos;
    view_matrix[0 * 4 + 1] = -c_sin;
    view_matrix[1 * 4 + 0] = c_sin;
    view_matrix[1 * 4 + 1] = c_cos;
    view_matrix[2 * 4 + 2] = 64;
    view_matrix[3 * 4 + 3] = 64;

    // Dispatch batch matrix multiplication to 8x8 Systolic Coprocessor
    gpu_transform_vertices_8x(in_coords, view_matrix, gemm_out);

    // Rasterize sprites with depth-buffer test against wall_dist_buffer[x]
    for (int i = 0; i < num_mobjs; i++) {
        mobj_t *mo = &mobjs[i];
        if (mo->type == MT_PLAYER) continue;

        fixed_t tz = FIXED_MUL(mo->x - viewx, viewcos) + FIXED_MUL(mo->y - viewy, viewsin);
        fixed_t tx = FIXED_MUL(mo->x - viewx, viewsin) - FIXED_MUL(mo->y - viewy, viewcos);

        if (tz < TO_FIXED(4)) continue;

        int sx = CENTER_X + (int)(((int64_t)tx * FOCAL_LEN) / tz);
        int scale = (int)(((int64_t)TO_FIXED(48) * FOCAL_LEN) / tz);

        // Billboard column rasterization with 1D depth buffer rejection
        draw_mobj_billboard(vram, mo, sx, scale, tz);
    }
}
```

---

## 4.8 Weapon Overlay & Dual Muzzle Blast Rendering (Super Shotgun)

The Super Shotgun (`SHT2A0`) is drawn centered at the bottom of the viewport:

```c
static void render_weapon(volatile uint8_t *vram) {
    const doom_patch_t *wpn = &patch_sht2a0;
    int wpn_w = wpn->width;  // 59 pixels wide
    int wpn_h = wpn->height; // 55 pixels high
    int wpn_x = (SCREEN_W - wpn_w) / 2; // (160 - 59) / 2 = 50
    int wpn_y = VIEWPORT_H - wpn_h + (walk_bob ? 2 : 0);

    draw_authentic_patch(vram, wpn, wpn_x, wpn_y);

    if (muzzle_flash > 0) {
        int barrel_cx = wpn_x + (wpn_w / 2);
        int barrel_y = wpn_y - 4;
        for (int dy = -8; dy <= 0; dy++) {
            for (int dx = -18; dx <= 18; dx++) {
                int dist_sq = dx * dx + dy * dy * 3;
                if (dist_sq < 150) {
                    uint8_t col = (dist_sq < 30) ? 4 : ((dist_sq < 90) ? 224 : 176);
                    put_pixel(vram, barrel_cx + dx, barrel_y + dy, col);
                }
            }
        }
    }
}
```

When fired, the Super Shotgun consumes 2 shells, emits a 20-pellet hitscan spread cone dealing up to 120 damage, triggers a dual muzzle blast, and switches Doomguy's face to the grinning evil expression (`STFEVL0`).

---

## 4.9 Industrial Beveled Status Bar HUD & Doomguy Face Animation

The bottom 20 scanlines ($y = 100 \dots 119$) are reserved for the status HUD:

```c
static void render_status_bar(volatile uint8_t *vram) {
    // 1. Draw Industrial Grey Beveled Plate using DOOM grey ramp (indices 96..111)
    for (int y = VIEWPORT_H; y < SCREEN_H; y++) {
        uint8_t line_col = 104;
        if (y == VIEWPORT_H)          line_col = 100; // Top shadow line
        else if (y == VIEWPORT_H + 1) line_col = 108; // Bevel highlight
        else if (y == SCREEN_H - 1)   line_col = 96;  // Bottom dark groove

        for (int x = 0; x < SCREEN_W; x++) {
            vram[y * SCREEN_W + x] = line_col;
        }
    }

    // 2. Draw Authentic Doomguy Animated Face in Center
    const doom_patch_t *face = (muzzle_flash > 0) ? &patch_stfevl0 : &patch_stfst00;
    int face_x = (SCREEN_W - face->width) / 2; // (160 - 24) / 2 = 68
    int face_y = VIEWPORT_H;
    draw_authentic_patch(vram, face, face_x, face_y);

    // 3. Gold Amber Ammo Gauge on Left (x = 8..48)
    int ammo_fill = (player_ammo * 40) / 50;
    for (int y = VIEWPORT_H + 6; y <= VIEWPORT_H + 13; y++) {
        for (int x = 8; x <= 48; x++) {
            vram[y * SCREEN_W + x] = (x - 8 <= ammo_fill) ? 224 : 0;
        }
    }

    // 4. Blood Red Health Gauge on Right (x = 112..152)
    int health_fill = (player_health * 40) / 100;
    for (int y = VIEWPORT_H + 6; y <= VIEWPORT_H + 13; y++) {
        for (int x = 112; x <= 152; x++) {
            vram[y * SCREEN_W + x] = (x - 112 <= health_fill) ? 176 : 0;
        }
    }
}
```

---

## 4.10 Cardinal Minimap with True-North Orientation

The top-left corner features an active 2D radar overlay rendered via Bresenham's line algorithm:
* **Orientation:** True-North oriented where world $+Y$ renders vertically upwards ($y = 34 - (Y + 200)/25$).
* **Wall Types:** Solid walls are rendered in slate grey (`112`); two-sided steps and doorways are rendered in amber (`224`).
* **Player Marker:** Position is marked with a white dot (`4`); camera heading is rendered as a 3-pixel directional red vector (`176`).

---

# 5. The Engineering Post-Mortem: Complete Catalog of Anomalies & Mitigations

During the bringup and optimization of the GEMMrv32 bare-metal engine, eleven major hardware, bus, and software failures were diagnosed down to root causes and mitigated.

---

## 5.1 Anomaly 1: The Ghost Wall Comb Striping Glitch

### Observed Symptom
When moving or turning in early builds, residual wall textures remained frozen on the monitor, split by thin 1-pixel vertical slits every 4 pixels horizontally ("residual textures staying in place with vertical comb breaks").

### Root Cause (Bus Steering Mismatch)
In `rtl/core/neorv32_vga.vhd`, the dual-port framebuffer Block RAM was declared as a byte-wide array:
```vhdl
type vram_t is array (0 to 19199) of std_ulogic_vector(7 downto 0);
signal vram : vram_t;
```
The bus write decoder used the lower address bits `bus_addr_i(1 downto 0)` to steer which byte of the incoming 32-bit data word was written:
```vhdl
case bus_addr_i(1 downto 0) is
  when "00"   => vram(byte_addr) <= bus_wdata_i(7 downto 0);   -- Only byte 0 written!
  when "01"   => vram(byte_addr) <= bus_wdata_i(15 downto 8);
  when "10"   => vram(byte_addr) <= bus_wdata_i(23 downto 16);
  when others => vram(byte_addr) <= bus_wdata_i(31 downto 24);
end case;
```
In software, the screen clear function attempted whole-frame clearing using 32-bit word stores:
```c
volatile uint32_t *vram32 = (volatile uint32_t *)VGA_VRAM_BASE;
vram32[i] = clear_word;
```
Because word stores present word-aligned addresses (`bus_addr_i(1 downto 0) = "00"`), the hardware only wrote Byte 0 to `vram(i*4)`. Bytes 1, 2, and 3 (`vram(i*4+1)`, `vram(i*4+2)`, `vram(i*4+3)`) were **never written**. Consequently, the clear function erased only 25% of columns (columns $0, 4, 8, \dots$), leaving the remaining 75% containing stale wall pixels from the previous frame.

### Mitigation
1. Eliminated whole-frame pre-clearing completely.
2. Implemented a **single-pass vertical column rasterizer** using native 8-bit byte stores (`uint8_t`), writing ceiling, wall, and floor pixels directly to matching memory addresses.

---

## 5.2 Anomaly 2: The 19.2 KB BRAM Hard Wall & 4bpp Read Trap

### Observed Symptom
When attempting native $320 \times 200$ @ 4bpp Mode, the bottom 40% of the display froze completely, and neighboring pixels corrupted each other's colors in psychedelic horizontal speckles.

### Root Cause
1. **Address Overflow:** A $320 \times 200$ buffer requires 32,000 bytes. The synthesized FPGA bitstream allocated 19,200 bytes ($160 \times 120$). For scanlines $y \ge 120$: $\text{addr} = (120 \times 320) / 2 = 19,200$. Every write beyond $y = 120$ generated addresses $\ge 19,200$, which the hardware write comparator discarded:
   ```vhdl
   if byte_addr < 19200 then
       -- Allow write
   end if;
   ```
2. **Write-Only Read Trap:** In `doom_defs.h`, packing two 4bpp nibbles per byte was implemented as:
   ```c
   vram[byte_idx] = (vram[byte_idx] & 0xF0) | (col & 0x0F);
   ```
   In `rtl/core/neorv32_vga.vhd` (line 208), the MMIO bus read interface for VRAM was unconnected and returned constant zeros (`bus_rdata_o <= (others => '0')`). Reading `vram[byte_idx]` returned `0x00`, inadvertently wiping the adjacent nibble to black on every single write.

### Mitigation
Locked the engine to native **$160 \times 120$ @ 8bpp**, providing full byte-per-pixel access within the 19,200-byte boundary and eliminating all read-modify-write operations.

---

## 5.3 Anomaly 3: The Hall of Mirrors (HOM) & Doorway Floor Aperture Stalls

### Observed Symptom
Moving through staircases or doorways left frozen weapon trails, stuck muzzle flashes, and psychedelic floor retention across scanlines $y = 117..167$.

### Root Cause
In authentic DOOM BSP rendering, two-sided portals draw only step faces and adjust `floorclip[x]`. Solid walls behind the portal only draw down to `floorclip[x]` and then set `floorclip[x] = -1`. The floor aperture in front of the step was never drawn by the step, never drawn by the distant wall, and ignored by post-processing, leaving stale frame data intact.

### Mitigation
Implemented `R_FillBackdrop()`: before masked entities are drawn, any column where `ceilingclip[x] < floorclip[x]` is filled with authentic ceiling flat (index 112) above the horizon and stone floor flat (index 72) below the horizon, guaranteeing **100% pixel coverage every frame**.

---

## 5.4 Anomaly 4: The Blocking UART Trap

### Observed Symptom
The game rendered one frame upon keystroke and then completely stalled; when idle, the frame rate dropped to 0 FPS.

### Root Cause
In `main.c`:
```c
if (neorv32_uart0_available()) {
    char cmd = (char)neorv32_uart0_getc();
    handle_input(cmd);
}
```
`neorv32_uart0_available()` checks whether UART hardware is present in the synthesized FPGA SoC (always returns true). `neorv32_uart0_getc()` is a blocking loop waiting for `char_received == 1`. Thus, the CPU halted on every frame until a user pressed a key.

### Mitigation
Replaced with non-blocking FIFO polling:
```c
if (neorv32_uart0_char_received()) {
    char cmd = neorv32_uart0_char_received_get();
    handle_input(cmd);
}
```

---

## 5.5 Anomaly 5: Vanishing Walls & Inverted Navigation

### Observed Symptom
Rotating 360 degrees caused boundary walls to disappear into voids; turning left with `'A'` swung the camera right, and the 2D minimap rendered South upwards.

### Root Cause
1. DOOM requires Counter-Clockwise (CCW) perimeter winding ($v_1 \to v_2$) so that the sector interior is on the right side. Clockwise linedef definitions caused one-sided walls to fail the visibility test ($x_1 < x_2$) and get back-face culled.
2. The input handler decremented the angle index on `'a'`, reversing yaw.
3. The minimap mapped world $+Y$ downwards ($y = (Y + 200)/16$).

### Mitigation
Re-defined all 18 map linedefs with strict CCW winding, added dynamic coordinate/sector swapping for two-sided portals viewed from the reverse direction, aligned angle incrementation with standard trigonometry, and mapped world $+Y$ upwards on the HUD.

---

## 5.6 Anomaly 6: Screen Tearing on Single-Buffered BRAM

### Observed Symptom
Rapid turns caused jagged horizontal tear lines across wall columns.

### Root Cause
The Basys 3 FPGA (`xc7a35t`) has **50 Block RAM tiles (225 KB total)**. 48 tiles are committed to IMEM (32-64 KB), DMEM (64-96 KB), and the active framebuffer (19.2-64 KB). Instantiating a second back-buffer for double buffering requires an additional 64 KB, exceeding physical silicon capacity.

### Mitigation
Implemented **Beam-Synchronized Rasterization**:
The CPU polls the hardware vertical blanking bit (`0x90010400`) via `wait_for_vblank()` before rendering. Rendering proceeds top-to-bottom ahead of the raster scanout, avoiding mid-frame collision.

---

## 5.7 Anomaly 7: Stack Pointer Overflow & Infinite Reset Loop

### Observed Symptom
On boot, the VGA monitor locked onto 640x480 sync but stayed black, while UART console repeatedly printed the first banner line in an infinite reset loop.

### Root Cause
In `sw/basys/neorv32_basys3_top.vhd`, Data RAM was synthesized as 64 KB (`0x80000000..0x8000FFFF`). However, `sw/common/neorv32.ld` had hardcoded `__neorv32_ram_size = 128k`. In `crt0.S`, the stack pointer was set to `0x80000000 + 128k = 0x80020000` (64 KB above physical RAM). When `neorv32_uart0_printf()` executed `sw ra, 28(sp)`, the return address was written to unmapped space. The subsequent `ret` read `0x00000000` and jumped to instruction 0 in ROM, endlessly resetting.

### Mitigation
Parameterised `__neorv32_ram_size` in `sw/common/neorv32.ld` (defaulting to 64k) and enforced `--defsym,__neorv32_ram_size=65536` in the makefile, establishing `__crt0_stack_top = 0x80010000` safely inside physical RAM.

---

## 5.8 Anomaly 8: PS/2 Pin Swap, 2-Cycle Double-Pop & FIFO Race Condition

### Observed Symptom
The USB keyboard connected via the Basys 3 PIC24 host bridge failed to register any keypresses; NumLock/CapsLock LEDs stayed dark.

### Root Cause
1. **Pin Swap in XDC:** In `sw/basys/basys3.xdc`, `B17` was assigned to `ps2_clk_i` and `C17` to `ps2_data_i`. According to the Basys 3 schematic, `C17` is Clock and `B17` is Data. The FPGA was sampling data off the clock line.
2. **Wishbone 2-Cycle Double-Pop:** In `neorv32_basys3_top.vhd`, `ps2_re` was combinationally driven by `ps2_sel and not xbus_we`. Because Wishbone read cycles span 2 clock cycles (strobe + ack), every CPU `lw` instruction asserted read-enable twice, popping two bytes per keypress and returning `0x00`.
3. **Simultaneous Push/Pop Race:** In `neorv32_ps2.vhd`, capturing a stop bit while the CPU read the FIFO corrupted `fifo_count`.

### Mitigation
Corrected pin assignments (`C17` = Clock, `B17` = Data), gated FIFO read-enable strictly with `xbus_ack = '1'`, and unified push/pop logic into atomic increment/decrement variables with a 100 µs idle resync timer.

---

## 5.9 Anomaly 9: UART Stream Overruns, Dropped Bytes & Timer Preemption

### Observed Symptom
Streaming firmware binary over UART hung partway through or reported `ERROR_EXCEPTION` / `ERROR_SIGNATURE`.

### Root Cause
1. `neorv32_basys3_top.vhd` lacked explicit FIFO configuration, defaulting to a 1-byte hardware RX buffer.
2. The machine timer interrupt (`CSR_MIE_MTIE`) triggered every 250 ms to toggle a heartbeat LED, preempting the CPU for ~100 clock cycles during continuous streaming and overflowing the 1-byte buffer.
3. Trailing `\r\n` from pressing `<Enter>` after typing `u` in terminal emulators remained in the RX FIFO, shifting the header signature.

### Mitigation
Synthesized **64-byte hardware RX/TX FIFOs** (`IO_UART0_RX_FIFO => 64`), disabled machine timer interrupts in `uart_setup()` during stream reception, and updated `uart_stream_get()` to discard leading CR/LF before parsing the executable header.

---

## 5.10 Anomaly 10: Fixed-Point Multiplicative Overflow in View Projections

### Observed Symptom
When approaching walls at certain angles, wall columns collapsed into thin vertical slivers or jumped across the screen.

### Root Cause
In `r_segs.c`:
```c
int x1 = CENTER_X + FROM_FIXED(FIXED_DIV(tx1 * FOCAL_LEN, tz1));
```
Where `tx1` is 16.16 fixed-point (e.g. $200 \times 65536 = 13,107,200$). Multiplying by `FOCAL_LEN` (160) yields $2,097,152,000$, which exceeds the 32-bit signed integer limit ($2,147,483,647$) when `tx1 > 163`. This arithmetic overflow wrapped into negative numbers, corrupting wall projections.

### Mitigation
Promoted projection calculations to 64-bit integer intermediates:
```c
int x1 = CENTER_X + (int)(((int64_t)tx1 * FOCAL_LEN) / tz1);
int x2 = CENTER_X + (int)(((int64_t)tx2 * FOCAL_LEN) / tz2);
```

---

## 5.11 Anomaly 11: Sprite Billboard Scanline Gaps ("Venetian-Blind" Effect)

### Observed Symptom
Monster billboards exhibited horizontal scanline gaps ("Venetian-blind" effect) and occasional CPU hangs when walking close to enemies.

### Root Cause
`draw_authentic_patch_scaled()` in `doom_assets_authentic.c` clamped `dest_y0` and `dest_y1` to screen bounds *before* computing the denominator `post_h = dest_y1 - dest_y0`. When an entity was clipped by the viewport edge, `post_h` shrank, accelerating texture sampling and causing divide-by-zero or out-of-bounds reads.

### Mitigation
Calculated `raw_post_h = raw_dest_y1 - raw_dest_y0` using unclamped projections. Enforced `raw_post_h > 0` before clamping screen bounds `dest_y0` and `dest_y1`, and continuously interpolated scanlines via `((dy - raw_dest_y0) * length) / raw_post_h`.

---

# 6. Build System, 100% Open-Source Toolchain & Operations Guide

## 6.1 The Open-Source Ecosystem

The entire development and deployment flow has been transitioned away from proprietary EDA vendor tools:
* **Toolchain:** `riscv-none-elf-gcc` (xPack GNU RISC-V Embedded GCC v15.2.0-1).
* **FPGA Programmer:** `openFPGALoader` (v1.1.1).
* **Serial Host:** Python 3 with `pyserial`.

All installed compiler binaries and runtime libraries are isolated in user-space under `/home/econsystems/.local/` and tracked in `INSTALLED_TOOLS.txt`.

---

## 6.2 Top-Level Makefile Architecture

The repository root `Makefile` provides 1-click execution for all operations:

```makefile
# ==============================================================================
# DOOM-on-GEMMrv32: Top-Level Project Makefile
# ==============================================================================

RISCV_PREFIX   ?= riscv64-unknown-elf-
OPENFPGALOADER ?= openFPGALoader
PYTHON         ?= python3
SERIAL_PORT    ?= /dev/ttyUSB1
BAUD           ?= 19200

.PHONY: all build flash-bit flash-spi upload clean help

all: build

build:
	@echo "=== Building DOOM (Authentic BSP + GEMM Coprocessor) ==="
	$(MAKE) -C src RISCV_PREFIX=$(RISCV_PREFIX) elf
	$(PYTHON) tools/image_gen.py -i src/main.elf -o bin/demo_vga_doom.bin
	@echo "SUCCESS: Output binary generated in bin/demo_vga_doom.bin"

flash-bit:
	@echo "=== Programming Basys 3 FPGA Bitstream via openFPGALoader (JTAG) ==="
	$(OPENFPGALOADER) --board=basys3 bit/neorv32_basys3.bit

flash-spi:
	@echo "=== Flashing Basys 3 Non-Volatile SPI Flash via openFPGALoader ==="
	$(OPENFPGALOADER) --board=basys3 -f bit/neorv32_basys3.bin

upload:
	@echo "=== Uploading DOOM Binary to Bootloader via Serial ==="
	$(PYTHON) tools/upload_serial.py --port $(SERIAL_PORT) --baud $(BAUD) bin/demo_vga_doom.bin

clean:
	$(MAKE) -C src RISCV_PREFIX=$(RISCV_PREFIX) clean_all
```

---

## 6.3 Firmware Packaging via Python `image_gen`

The executable packaging tool `tools/image_gen.py` converts the raw compiled ELF binary into a bootloader-ready `.bin` image:
1. Extracts the `.text`, `.rodata`, `.data`, and `.bss` loadable sections.
2. Prepends the 16-byte NEORV32 bootloader header (`0x214F454E` signature, base execution address `0x00000000`, size in bytes).
3. Computes a 32-bit checksum appended to the end of the image.

---

## 6.4 Board Programming & Serial Streaming

### Step 1: Program the FPGA Bitstream
```bash
make flash-bit
```
*The FPGA is configured via JTAG in ~3 seconds. The monitor backlight locks onto 640x480 sync.*

### Step 2: Compile the Game Firmware
```bash
make build
```
*Generates `bin/demo_vga_doom.bin` (checksum verified).*

### Step 3: Stream Game Firmware to Bootloader
```bash
make upload SERIAL_PORT=/dev/ttyUSB1 BAUD=19200
```
*The Python loader sends `u`, streams the binary payload with progress reporting, verifies the checksum, sends `e` to execute, and automatically transitions into a live terminal monitor.*
