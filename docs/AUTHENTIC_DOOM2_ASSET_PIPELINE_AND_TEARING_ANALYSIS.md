# Authentic DOOM 2 Asset Pipeline, RP2040 DOOM Architecture & Display Tearing Analysis

## 1. Executive Summary

This document addresses two core technical issues raised during the evaluation of the bare-metal RISC-V DOOM engine on the Digilent Basys 3 FPGA:
1. **Why the Visual Assets Looked Artificial**: Clarification of how Graham Sanderson's RP2040 DOOM handles authentic Id Software art assets versus the procedural placeholder C arrays currently compiled in the repository.
2. **The Cause of Screen Tearing**: Complete hardware, bus, and timing analysis of screen tearing on a 100% capacity Block RAM single-buffered framebuffer.
3. **Hardware Boundaries & The Path to True DOOM 2**: Concrete architectural constraints of the Digilent Basys 3 (`xc7a35tcpg236-1`) and the pipeline required to deploy genuine Id Software sprites, textures, and tear-free rasterization.

---

## 2. Graham Sanderson's RP2040 DOOM: The Asset Reality

### A. The Open-Source vs. Commercial Copyright Boundary
A common misconception is that open-source DOOM ports (such as `chocolate-doom`, `prboom`, or `rp2040-doom`) come pre-packaged with DOOM's art and sound files. 
* **The Code (GPL-2.0)**: John Carmack open-sourced the DOOM source code in 1997. Anyone can legally modify, compile, and distribute the C source code.
* **The Game Assets (Proprietary / Copyright Id Software / ZeniMax)**: The artwork (sprites, textures, flats, status bar graphics) and audio files created by Adrian Carmack, Kevin Cloud, and Bobby Prince **remain commercial intellectual property**. Distributing them on a public GitHub repository violates copyright law.

### B. How Graham Sanderson's Port Obtains True Sprites
Graham Sanderson's repository (`kilograham/rp2040-doom`) contains **zero game sprites or textures**. Instead, it uses a host-side extraction and compilation utility called **`whd_gen`**:
1. The end-user supplies their own legally acquired `DOOM1.WAD` (Shareware) or `DOOM2.WAD` (Commercial).
2. `whd_gen` parses the binary IWAD directory, identifies every sprite, wall patch, flat, and sound lump, compresses the data into custom `WHD` (Where's Half the Data?) or `WHX` containers, and generates a flashable binary image.
3. The Raspberry Pi Pico streams these genuine Id Software sprites directly from QSPI Flash using execute-in-place (XIP).

### C. Why Our Previous Builds Used Synthetic Placeholders
In our codebase (`src/v1`, `src/v2`, `src/v3`, `src/v4`), the graphics were generated via procedural algorithm arrays (`doom_assets_4bpp.c`):
* Wall textures were generated using geometric XOR checkerboards and synthetic brick stripes.
* Monsters and weapons were hard-coded 32x32 pixel geometric approximations.
* This explains why the engine did not visually resemble authentic DOOM 2.

---

## 3. Screen Tearing Technical Root Cause Analysis

### A. Single-Buffered Framebuffer Mechanics
A standard CRT or LCD monitor operating in standard $640 \times 480$ @ 60 Hz VGA mode scans out pixels continuously:
* **Frame Period**: $16.67\text{ ms}$ (60 frames per second).
* **Scanline Period**: $31.77\ \mu\text{s}$ (31.47 kHz horizontal frequency).
* **Active Video Scan**: The electron beam or LCD raster controller reads from the top-left pixel $(0, 0)$ to the bottom-right pixel $(319, 199)$ in sequential raster order.

```
       Scanout Beam Position
                |
                v
+---------------+-------------------+
|               |   ALREADY SCANNED |
|               |   (Frame N)       |
+---------------+-------------------+  <--- TEAR LINE (CPU writes new frame here)
|               |   NOT YET SCANNED |
|               |   (Frame N-1)     |
+---------------+-------------------+
```

### B. Why Tearing Occurs in Our System
1. **Direct VRAM Writes**: The NEORV32 RISC-V CPU writes wall and sprite columns directly into the single 64,000-byte VRAM array at `0x90000000`.
2. **Asynchronous Execution**: The CPU renders a frame over tens of thousands of clock cycles without coordinating with the VGA raster counter (`v_cnt`).
3. **Mid-Frame Overwriting**: If the CPU renders a wall column at $x = 100$ while the VGA beam is scanning across line $y = 80$, the top part of the column shows the new frame while the bottom part shows the previous frame, creating a jagged horizontal tear line.

### C. Why We Cannot Simply "Double Buffer" in Block RAM
To eliminate tearing via double buffering, the hardware requires two full framebuffers:
* **Buffer A (Front Buffer)**: Read by the VGA scanout logic.
* **Buffer B (Back Buffer)**: Written by the CPU.
* **Buffer Size**: $320 \times 200 = 64,000\text{ bytes}$ each $\rightarrow 128,000\text{ bytes total}$.

#### The Hard FPGA Block RAM Limit:
* Digilent Basys 3 FPGA (`xc7a35tcpg236-1`) total BRAM: **50 BRAM36 tiles (225 KB total)**.
* **Current Allocation**:
  * 16 tiles = 64 KB Framebuffer (Front buffer)
  * 24 tiles = 96 KB Data RAM (DMEM)
  * 8 tiles = 32 KB Instruction RAM (IMEM)
  * 2 tiles = Bootloader ROM & FIFO buffers
  * **Total**: **50 / 50 tiles (100.00% utilization)**.
* **Hard Constraint**: There are **zero Block RAM tiles remaining** to instantiate a second 64 KB buffer.

---

## 4. Hardware Constraints Comparison: Basys 3 vs. Nexys A7

| Specification | Digilent Basys 3 (`xc7a35t`) | Digilent Nexys A7 (`xc7a100t`) | Impact on DOOM 2 |
| :--- | :--- | :--- | :--- |
| **External RAM** | **0 MB (None)** | **128 MB DDR2 SDRAM** | On Basys 3, all RAM must fit in FPGA Block RAM (225 KB). On Nexys A7, full unmodified LinuxDoom runs natively in DDR2. |
| **Internal Block RAM** | 50 tiles (225 KB) | 135 tiles (607 KB) | Basys 3 cannot double-buffer 320x200 @ 8bpp. Nexys A7 can easily double- or triple-buffer. |
| **BRAM Utilization** | **100% (50/50 tiles)** | ~35% | Basys 3 is at the absolute physical silicon capacity limit. |
| **Onboard SPI Flash** | 4 MB (`MX25L3273F`) | 16 MB (`S25FL128S`) | Basys 3 has 2.55 MB usable flash after bitstream. Nexys A7 has ~14.5 MB usable flash (can hold entire unmodified `DOOM2.WAD`). |
| **Audio Hardware** | Resistor mono / PWM | Mono audio amp + PWM / Pmod I2S | Basys 3 requires external speaker or PWM header; Nexys A7 has built-in audio jack. |

---

## 5. Technical Roadmap to True DOOM 2 on Basys 3

### Phase 1: Eliminate Screen Tearing Without a Second BRAM Buffer
Since a second 64 KB BRAM buffer is physically impossible on the Basys 3, tearing must be resolved via **VBlank Synchronization**:
1. **Hardware VBlank Status**: In `neorv32_vga.vhd`, register `0x90010400` bit 0 reports when the VGA beam enters vertical blanking (lines 480 to 524, lasting $\approx 1.4\text{ ms}$).
2. **Software Wait-for-VBlank**:
   ```c
   static inline void wait_for_vblank(void) {
       volatile uint32_t *vga_stat = (volatile uint32_t *)0x90010400;
       while (!(*vga_stat & 1)); // Wait for VBlank start
       while (*vga_stat & 1);    // Wait for VBlank end
   }
   ```
3. **Beam-Synchronized Rasterization**: By triggering frame updates at the start of VBlank and rendering top-to-bottom, the CPU draws *ahead* of the raster beam rather than colliding with it.

### Phase 2: Authentic DOOM 2 Asset Extraction Pipeline
To replace the synthetic placeholders with genuine 1994 Id Software graphics:
1. **Lump Extraction Tool (`wad_extract.py`)**:
   Parse commercial `DOOM2.WAD` and extract authentic binary lumps:
   * **Super Shotgun**: `SHT2A0`, `SHT2B0`, `SHT2C0`, `SHT2D0`, `SHT2E0`, `SHT2F0`, `SHT2G0`, `SHT2H0`, `SHT2I0`, `SHT2J0`.
   * **Monsters**:
     * Shotgun Guy: `SPOSA1` through `SPOSD1` (Walk), `SPOSE1` (Attack), `SPOSF1` (Pain), `SPOSG0`–`SPOSL0` (Death).
     * Imp: `TROOA1` through `TROOD1` (Walk), `TROOE1`–`TROOG1` (Attack), `TROOH1` (Pain), `TROOI0`–`TROON0` (Death).
     * Pinky Demon: `SARGA1` through `SARGD1` (Charge), `SARGE1` (Bite), `SARGF1` (Pain), `SARGG0`–`SARGL0` (Death).
   * **Wall Textures & Flats**: `STARTAN3`, `BROWN1`, `STEP4`, `DOOR3`, `EXITDOOR`, `FLOOR4_8`, `CEIL3_5`.
   * **Status Bar HUD**: Authentic metal backing plate (`STBAR`), number fonts (`STTNUM0`..`9`), and animated Doomguy face (`STFST00`..`STFDEAD0`).
2. **Format Conversion**:
   Convert Id Software's column-post format into linear indexed 8bpp byte arrays.
3. **Flash Deployment**:
   Compile the extracted lumps into an asset pack placed in the free **2.55 MB of onboard QSPI flash** or directly linked into the application image.
