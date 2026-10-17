# DOOM-on-GEMMrv32: Standalone FPGA Computer Architecture & GPU Plan

This document outlines the architecture, hardware modifications, C driver specifications, and software porting roadmap to run **Doom** (and interactive graphics applications) on the **Digilent Basys 3** using the **GEMMrv32** SoC.

---

## 1. System Vision & Paradigm

The system operates as a **true standalone retro-workstation**:
* **Host CPU**: NEORV32 RV32IM processor running at 50 MHz.
* **GPU / Vector Unit**: Custom 8x8 Systolic-Array Coprocessor + 256-bit DMA engine.
* **Display Output**: 12-bit resistor DAC VGA port driving a 60 Hz color display.
* **User Input**: USB Keyboard connected to the Basys 3 PIC24 host bridge (scanned via PS/2).
* **Firmware & Doom Engine**: Bare-metal C port with custom GPU blitting, column drawing, and affine/matrix routines.

```
       +--------------------------------------------------------------------+
       |                           Basys 3 FPGA                             |
       |                                                                    |
       |  [USB Keyboard] ---> PIC24 Bridge ---> PS/2 RX Controller (MMIO)   |
       |                                                |                   |
       |                                                v                   |
       |  +-------------------+        +---------------------------------+  |
       |  |  NEORV32 CPU      |        |        NEORV32 Memory Bus       |  |
       |  |  RV32IM @ 50 MHz  |<======>|   - 128 KB Instruction BRAM     |  |
       |  +-------------------+        |   - 96 KB Data BRAM             |  |
       |                               +---------------------------------+  |
       |                                                |                   |
       |               +--------------------------------+                   |
       |               |                                |                   |
       |               v                                v                   |
       |  +-------------------------+      +--------------------------+     |
       |  |    GEMM Accelerator     |      |      VGA Controller      |     |
       |  |     (CFS "GPU Unit")    |      |    (Dual-Port VRAM)      |     |
       |  |  - 8x8 Systolic MAC     |      |  - 160x120 or 320x200    |     |
       |  |  - 256-bit Wide DMA     |      |  - Palette Lookup (256c) |     |
       |  |  - Matrix/Vector Engine |      |  - 60 Hz Sync Generator  |     |
       |  +-------------------------+      +--------------------------+     |
       |               |                                |                   |
       |               +-----> [Hardware Blits] --------+                   |
       |                                                |                   |
       +------------------------------------------------|-------------------+
                                                        v
                                                 [12-bit VGA DAC]
                                                 (Red, Green, Blue,
                                                  H-Sync, V-Sync)
```

---

## 2. Hardware Resource Budget (Artix-7 XC7A35T)

The Basys 3 uses an Artix-7 `XC7A35T-1CPG236C`, which has **50 Block RAMs (36 Kb each = 225 KB max)**.

| Subsystem | RAM Allocated | Implementation Details |
|---|---|---|
| **Instruction ROM/RAM (IMEM)** | **128 KB** | Houses the Doom executable code and bootloader. |
| **Data RAM (DMEM)** | **64 KB - 96 KB** | Dynamic heap, game state, bsp nodes, sound tables. |
| **Video Framebuffer (VRAM)** | **19.2 KB** | **160x120 @ 8bpp** (upscaled 4x to 640x480) OR **320x200 @ 4bpp** banked. |
| **Color Palette (CLUT)** | **512 Bytes** | 256 entries $\times$ 12-bit RGB color lookup table in distributed LUT RAM. |
| **Total BRAM Used** | **~212 KB / 225 KB** | **Feasible on XC7A35T without external DRAM.** |

> [!NOTE]
> Why 160x120 or 320x200? Standard Doom renders internally at 320x200. A 160x120 8-bit framebuffer requires only 19,200 bytes, leaving the remaining memory for the Doom engine and WAD data.

---

## 3. Hardware Modules to Implement

### A. VGA Raster Controller (`rtl/core/neorv32_vga.vhd`)
- Generates standard 640x480 @ 60 Hz timing signals:
  - Pixel Clock: **25.0 MHz** (derived directly from the top MMCM).
  - Horizontal: 640 active, 16 front porch, 96 sync, 48 back porch (800 total).
  - Vertical: 480 active, 10 front porch, 2 sync, 33 back porch (525 total).
- **Pixel Scaler**: Replicates each pixel 4x horizontally and 4x vertically so a compact 160x120 buffer fills the 640x480 screen.
- **Dual-Port Memory**:
  - Port A (CPU / GPU write): 32-bit Wishbone/MMIO slave.
  - Port B (Display read): High-speed scanout reading palette indexed pixels.
- **Palette Engine (CLUT)**:
  - 256 color registers (12-bit RGB: 4-bit R, 4-bit G, 4-bit B).
  - Allows full Doom colormap and palette shifting (e.g., screen flashes red when hurt, green with radiation suit).

### B. PS/2 Keyboard Controller (`rtl/core/neorv32_ps2.vhd`)
- Connects to pins `B17` (Clock) and `C17` (Data).
- Decodes standard 11-bit PS/2 frames (1 start bit, 8 data bits, parity, 1 stop bit).
- Exposes a 16-byte FIFO to the CPU:
  - Status Register: `Data Ready`, `FIFO Full`, `Parity Error`.
  - Data Register: Scancode byte.

### C. Pin Constraints (`sw/basys/basys3.xdc`)
Assign physical pins for the 14 VGA signals and 2 PS/2 signals.

---

## 4. GEMM Unit as a 2.5D/3D GPU Accelerator

Doom's renderer is based on:
1. **BSP Traversal & 2D Projection**: Calculating wall segment angles and distances.
2. **Column Wall Drawing (`R_DrawColumn`)**: Vertical texture-mapped column blits.
3. **Span Floor/Ceiling Drawing (`R_DrawSpan`)**: Horizontal texture-mapped floor flats.

### GPU Acceleration Modes via `gemmrv_gpu.h`:
* **Mode 0: Fast Block Transfer & Clear (Hardware Blit)**:
  - Using the 256-bit DMA engine to clear the framebuffer or copy background HUD graphics in 32-byte chunks per cycle (8x faster than CPU 32-bit `memset`).
* **Mode 1: Texture Column Interpolator**:
  - The 8x8 systolic array computes 8 parallel affine/fixed-point step calculations simultaneously for wall texture coordinates $(u, v)$.
* **Mode 2: 3D Vector & Vertex Transformations**:
  - For sprites, 3D object models, or mini-games: batches $4 \times 4$ projection/view transformation matrices over 8 vertices per tile dispatch.

---

## 5. Software Driver API (`sw/example/gemm_lib/gemmrv_gpu.h`)

```c
#ifndef GEMMRV_GPU_H
#define GEMMRV_GPU_H

#include <stdint.h>

// Video Subsystem Base Addresses (MMIO)
#define VGA_VRAM_BASE       0x90000000  // Dual-port Framebuffer
#define VGA_PALETTE_BASE    0x90010000  // 256 x 12-bit Palette (CLUT)
#define VGA_CTRL_BASE       0x90010400  // Video Mode & Sync Register
#define PS2_DATA_REG        0x90020000  // Keyboard Scancode FIFO
#define PS2_STAT_REG        0x90020004  // Keyboard Status

// Display specifications
#define SCREEN_WIDTH        160
#define SCREEN_HEIGHT       120

// Video Controller functions
void vga_init(void);
void vga_set_palette(const uint16_t *palette_12bit);
void vga_clear_screen(uint8_t color_index);
void vga_flip_buffer(void);

// GEMM GPU Accelerated Rendering Primitives
void gpu_blit_rect(int x, int y, int w, int h, const uint8_t *src_pixels);
void gpu_draw_wall_column(int x, int ytop, int ybottom, uint32_t tex_step, const uint8_t *texture);
void gpu_transform_vertices_8x(int16_t *in_verts, int16_t *matrix_4x4, int16_t *out_verts);

// Keyboard API
int ps2_has_key(void);
uint8_t ps2_get_scancode(void);
char ps2_scancode_to_ascii(uint8_t scancode);

#endif
```

---

## 6. Execution Roadmap

| Step | Milestone | Output |
|---|---|---|
| **Phase 1** | **I/O & Constraints Definition** | Update [basys3.xdc](sw/basys/basys3.xdc) with VGA (14 pins) and PS/2 (2 pins). |
| **Phase 2** | **VGA & Keyboard RTL Cores** | Write `neorv32_vga.vhd` (with 160x120 / 320x200 pixel scaling + palette) and `neorv32_ps2.vhd`. |
| **Phase 3** | **SoC Integration** | Wire VGA and PS/2 into [neorv32_basys3_top.vhd](sw/basys/neorv32_basys3_top.vhd). |
| **Phase 4** | **C Hardware Drivers** | Implement `gemmrv_gpu.c` and `gemmrv_ps2.c` in `sw/example/gemm_lib`. |
| **Phase 5** | **Graphics & Shell Sanity Demo** | A standalone C console firmware with text terminal, color bars, bouncing 3D cube, and keyboard echo. |
| **Phase 6** | **Doom Port (Doom-Tiny / Pure-Doom Baremetal)** | Adapt a minimal C Doom engine (e.g. PureDoom / Generic Doom) replacing `I_Video` and `I_Input` with `gemmrv_gpu` routines. |
