# GEMMrv32-DOOM Hardware Graphics & Standalone Workstation Architecture

This document provides a comprehensive technical overview of the graphics controller, keyboard peripheral, hardware GPU accelerator, and C driver subsystem implemented for the **GEMMrv32-DOOM!** standalone FPGA computer architecture.

---

## 1. Hardware Architecture Overview

The system enhances the base NEORV32 RISC-V SoC into a fully autonomous, standalone retro-workstation:

- **Host Processor**: NEORV32 RV32IM processor clocked at 50 MHz.
- **External Bus Interconnect (XBUS)**: 32-bit Wishbone/MMIO pipeline connecting custom graphics and I/O devices.
- **GPU Accelerator (Custom Functions Subsystem - CFS)**: 8x8 systolic-array MAC coprocessor with 256-bit DMA interconnect for matrix transformations and fast block memory blitting.
- **Video Display Controller**: Custom RTL framebuffer scanning out to a 12-bit resistor DAC driving standard 640x480 @ 60 Hz VGA displays.
- **Keyboard Interface**: Digital deglitched PS/2 frame receiver with an on-chip hardware FIFO.

```
       +-------------------------------------------------------------------------+
       |                        GEMMrv32-DOOM SoC Architecture                   |
       |                                                                         |
       |  +--------------------+                     +------------------------+  |
       |  |  NEORV32 RV32IM    |<== 32-bit Bus =====>|    Peripheral Bus      |  |
       |  |  CPU @ 50 MHz      |                     |    UART0, GPIO, CLINT  |  |
       |  +---------+----------+                     +------------------------+  |
       |            |                                                            |
       |            +==================== XBUS (MMIO) ===================+       |
       |            |                                                    |       |
       |            v                                                    v       |
       |  +---------------------+                             +----------------+ |
       |  |  neorv32_cfs (GPU)  |                             |  neorv32_vga   | |
       |  |  8x8 Systolic Array |                             |  160x120 VRAM  | |
       |  |  256-bit DMA        |                             |  256c Palette  | |
       |  +----------+----------+                             +--------+-------+ |
       |             |                                                 |         |
       |             +------------ [Hardware Blits] ------------------>+         |
       |                                                               |         |
       |  +---------------------+                                      v         |
       |  |  neorv32_ps2        |                                [12-bit DAC]    |
       |  |  Deglitch & FIFO    |                                (R, G, B, Sync) |
       |  +----------+----------+                                      |         |
       +-------------|-------------------------------------------------|---------+
                     ^                                                 v
           [USB Keyboard (B17, C17)]                             [VGA Monitor]
```

---

## 2. Memory-Mapped I/O (MMIO) Allocation

All graphics and keyboard peripherals are routed to the upper un-cached address map (`0x90000000` - `0x9002FFFF`):

| Address Range | Size | Peripheral | Access | Description |
|---|---|---|---|---|
| `0x90000000` - `0x90004AFF` | 19,200 B | **VGA VRAM** | R/W (8/32-bit) | 160x120 linear byte-indexed framebuffer. |
| `0x90010000` - `0x900103FF` | 1,024 B | **VGA Palette (CLUT)** | R/W (32-bit) | 256 color lookup entries (12-bit RGB each). |
| `0x90010400` | 4 B | **VGA Status Reg** | Read-Only | Bit `0`: `in_vblank` (Vertical Blanking Interval flag). |
| `0x90020000` | 4 B | **PS/2 Data Reg** | Read-Only | Pops and returns oldest scancode byte from FIFO. |
| `0x90020004` | 4 B | **PS/2 Status Reg** | Read-Only | Bit `0`: `data_available`, Bit `1`: `fifo_full`, Bits `[7:4]`: count. |

---

## 3. RTL Core Specifications

### A. `rtl/core/neorv32_vga.vhd`
- **Resolution**: 160x120 pixels, 8-bit color depth (256 simultaneous colors from a 4,096 color palette).
- **Pixel Clock**: 25.0 MHz (generated from the 50.0 MHz system clock via alternating cycle toggles).
- **Display Scanout**: Generates standard 640x480 @ 60 Hz timing ($H_{\text{total}} = 800$, $V_{\text{total}} = 525$).
- **Hardware Upscaling**: Replicates each logical pixel 4 times horizontally and 4 times vertically ($4\times 4$ hardware zoom), cleanly filling the entire screen without CPU overhead.
- **Dual-Port Video Memory**:
  - *Port A*: Synchronous 32-bit/8-bit write port connected to the NEORV32 system bus.
  - *Port B*: Synchronous internal read port accessed by the video raster engine.
- **Color Lookup Table (CLUT)**: 256 words $\times$ 12 bits implemented in distributed RAM. Format: `[11:8]` Red, `[7:4]` Green, `[3:0]` Blue.

### B. `rtl/core/neorv32_ps2.vhd`
- **Signal Filtering**: 8-stage majority digital deglitch filter on incoming `PS2_CLK` and `PS2_DATA` lines to reject contact bounce and EMI noise from USB-to-PS/2 conversion.
- **Frame Decoder**: State machine capturing 11-bit serial packets (1 Start bit, 8 Data bits, 1 Odd Parity bit, 1 Stop bit) sampled on the falling edges of the filtered clock.
- **FIFO Buffer**: 16-byte circular buffer with automatic read pointer advancement upon MMIO access.

---

## 4. C Driver API Reference (`gemmrv_gpu.h` / `gemmrv_gpu.c`)

### Video Display Functions

#### `void vga_wait_vsync(void)`
Blocks execution until the monitor enters vertical blanking. Used in animation loops to eliminate tearing.

#### `bool vga_is_vblank(void)`
Returns `true` if the raster gun is currently outside the active visible display region.

#### `void vga_set_palette_entry(uint8_t index, uint16_t rgb12)`
Programs a single 12-bit color in the hardware palette.
* **Parameters**:
  - `index`: Palette entry (`0` - `255`).
  - `rgb12`: Color value formatted as `(R << 8) | (G << 4) | B`, where each channel is `0` - `15`.

#### `void vga_set_palette(const uint16_t *palette_table)`
Loads a complete 256-color palette from a contiguous 512-byte array.

#### `void vga_clear(uint8_t color_idx)`
Clears the entire 160x120 screen using 32-bit burst writes (4 pixels packed per cycle).

#### `void vga_put_pixel(int x, int y, uint8_t color_idx)`
Plots a single pixel at coordinate $(x, y)$ with boundary safety checking.

#### `void vga_draw_hline(int x, int y, int length, uint8_t color_idx)`
Draws an optimized horizontal line segment with clipping.

#### `void vga_draw_vline(int x, int y, int length, uint8_t color_idx)`
Draws a vertical line segment with clipping.

#### `void vga_blit_rect(int x, int y, int w, int h, const uint8_t *src_pixels)`
Copies an arbitrary 2D bitmap rectangle into the framebuffer with boundary clipping.

---

### Hardware GPU Acceleration Functions

#### `void gpu_clear_screen(uint8_t color_idx)`
Fast memory-clear alias.

#### `void gpu_draw_wall_column(int x, int ytop, int ybottom, uint32_t tex_step, const uint8_t *texture, int tex_height)`
Hardware-assisted texture-mapping routine modeled directly on Doom's `R_DrawColumn`. Renders vertical spans of textured walls with fixed-point texture coordinate accumulation.

#### `void gpu_transform_vertices_8x(const int8_t *in_verts_8x4, const int8_t *matrix_4x4, int32_t *out_verts_8x4)`
Accelerates 3D and affine geometry by offloading vertex multiplication directly to the 8x8 systolic coprocessor.
- Batches 8 vertices simultaneously.
- Multiplies an $8 \times 4$ input matrix by a $4 \times 4$ view/projection matrix in hardware.
- Outputs 8 transformed 32-bit coordinates in parallel.

---

### Keyboard Driver Functions

#### `bool ps2_has_key(void)`
Returns `true` if one or more scancodes are waiting in the hardware FIFO.

#### `uint8_t ps2_get_scancode(void)`
Fetches the oldest scancode from the FIFO without blocking.

#### `uint8_t ps2_wait_scancode(void)`
Blocks execution until a key is pressed and returns its raw scancode.

#### `char ps2_scancode_to_ascii(uint8_t scancode)`
Decodes standard Set-2 make codes into ASCII characters (alphanumeric, space, enter, backspace). Returns `0` for unmapped or non-printable keys.

---

## 5. Physical Pinout Summary (Digilent Basys 3)

| Net Name | FPGA Pin | I/O Standard | Description |
|---|---|---|---|
| `vga_red_o[0]` | `G19` | LVCMOS33 | VGA Red bit 0 (LSB) |
| `vga_red_o[1]` | `H19` | LVCMOS33 | VGA Red bit 1 |
| `vga_red_o[2]` | `J19` | LVCMOS33 | VGA Red bit 2 |
| `vga_red_o[3]` | `N19` | LVCMOS33 | VGA Red bit 3 (MSB) |
| `vga_green_o[0]` | `J17` | LVCMOS33 | VGA Green bit 0 (LSB) |
| `vga_green_o[1]` | `H17` | LVCMOS33 | VGA Green bit 1 |
| `vga_green_o[2]` | `G17` | LVCMOS33 | VGA Green bit 2 |
| `vga_green_o[3]` | `D17` | LVCMOS33 | VGA Green bit 3 (MSB) |
| `vga_blue_o[0]` | `N18` | LVCMOS33 | VGA Blue bit 0 (LSB) |
| `vga_blue_o[1]` | `L18` | LVCMOS33 | VGA Blue bit 1 |
| `vga_blue_o[2]` | `K18` | LVCMOS33 | VGA Blue bit 2 |
| `vga_blue_o[3]` | `J18` | LVCMOS33 | VGA Blue bit 3 (MSB) |
| `vga_hsync_o` | `P19` | LVCMOS33 | VGA Horizontal Sync |
| `vga_vsync_o` | `R19` | LVCMOS33 | VGA Vertical Sync |
| `ps2_clk_i` | `B17` | LVCMOS33 (PULLUP) | PS/2 Clock from PIC24 Host Bridge |
| `ps2_data_i` | `C17` | LVCMOS33 (PULLUP) | PS/2 Data from PIC24 Host Bridge |
