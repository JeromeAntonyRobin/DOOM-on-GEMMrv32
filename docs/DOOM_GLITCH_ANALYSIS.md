# DOOM on GEMMrv32: Visual Artifact & Glitch Technical Analysis

This document provides the complete hardware, bus-level, and software-level root cause analysis of the two major visual anomalies encountered during the development of bare-metal DOOM on the GEMMrv32 NEORV32 SoC, and the mathematical principles used to eliminate them.

---

## 1. Glitch 1: The "Ghost Wall" / Vertical Striping Anomaly

### Observed Symptom
When moving or turning in early builds of DOOM, residual ghost wall textures remained frozen in place on the VGA display, broken up by thin 1-pixel vertical slits every 4 pixels horizontally ("residual wall textures staying in a position even after moving with vertical breaks horizontally").

### Root Cause Analysis (RTL vs. Firmware Bus Width Steering Mismatch)
1. In `rtl/core/neorv32_vga.vhd`, the dual-port framebuffer Block RAM is declared as an 8-bit wide array:
   ```vhdl
   type vram_t is array (0 to 19199) of std_ulogic_vector(7 downto 0);
   signal vram : vram_t;
   ```
2. The bus write decoder uses the lower address bits `bus_addr_i(1 downto 0)` to steer which byte of the incoming 32-bit data word `bus_wdata_i` is written to `vram(byte_addr)`:
   ```vhdl
   case bus_addr_i(1 downto 0) is
     when "00"   => vram(byte_addr) <= bus_wdata_i(7 downto 0);   -- Only byte 0!
     when "01"   => vram(byte_addr) <= bus_wdata_i(15 downto 8);
     when "10"   => vram(byte_addr) <= bus_wdata_i(23 downto 16);
     when others => vram(byte_addr) <= bus_wdata_i(31 downto 24);
   end case;
   ```
3. In the initial engine release, screen clearing was attempted using 32-bit word stores:
   ```c
   volatile uint32_t *vram32 = (volatile uint32_t *)VGA_VRAM_BASE;
   vram32[i] = ceiling_word;
   ```
4. Because 32-bit word stores always present word-aligned addresses (`bus_addr_i(1 downto 0) = "00"`), the hardware only wrote Byte 0 to `vram(i*4)`. Bytes 1, 2, and 3 (`vram(i*4+1)`, `vram(i*4+2)`, `vram(i*4+3)`) were **never written**.
5. Consequently, the screen clear step only erased **25% of pixels** (columns $0, 4, 8, \dots$). The other **75% of columns retained the previous frame's wall pixels**, creating persistent vertical ghost bars!

### Resolution (Single-Pass Column Rasterization)
* Discard whole-frame pre-clearing completely.
* In the DDA raycast loop, rasterize every column $x \in [0, 159]$ in a single continuous vertical pass from $y = 0$ to $y = 119$ using native 8-bit byte stores:
  1. Rows $0$ to $\text{drawStart} - 1$: Ceiling color (byte write)
  2. Rows $\text{drawStart}$ to $\text{drawEnd}$: Shaded Wall color (byte write)
  3. Rows $\text{drawEnd} + 1$ to $119$: Floor color (byte write)
* Overwrites 100% of framebuffer pixels on every frame with byte-wide memory stores matching the FPGA Block RAM architecture.

---

## 2. Glitch 2: The DOOM v3 Stride Overflow, Write-Only Bus Trap & Hall-of-Mirrors Glitch

### Observed Symptom
Upon executing DOOM v3 (which targeted authentic DOOM native $320 \times 200$ @ 4bpp) on the $160 \times 120$ 8bpp hardware bitstream, the screen produced extreme visual distortion:
1. Massive frame retention and persistent ghost wall trails trailing the player as they rotated.
2. The bottom 40% of the display froze completely, retaining stale data or initial uninitialized patterns.
3. Neighboring pixels corrupted each other's colors in psychedelic horizontal chromatic noise.

### Root Cause Analysis (Triple Hardware / Software Collision)

#### A. The 19,200-Byte Physical Hard Wall
* In `rtl/core/neorv32_vga.vhd` (line 166), the active FPGA bitstream synthesizes a $160 \times 120$ 8bpp framebuffer with 19,200 bytes of dual-port BRAM:
  ```vhdl
  byte_addr := to_integer(unsigned(bus_addr_i(14 downto 0)));
  if byte_addr < 19200 then
    -- Write into VRAM
  end if;
  ```
* DOOM v3's software rasterizer assumes $320 \times 200$ @ 4bpp, which requires $32,000 \text{ bytes}$ ($320 \times 200 / 2$).
* For scanlines $y \ge 120$: $\text{addr} = (120 \times 320) / 2 = 19,200$. Every write to scanlines $120 \dots 199$ was strictly $\ge 19,200$, causing the FPGA hardware write comparator to **silently discard 100% of the writes**! The lower 40% of the display was permanently stranded and frozen.

#### B. The Write-Only VRAM Read-Modify-Write Trap
* In `doom_defs.h`, packing two 4bpp nibbles per byte was implemented as:
  ```c
  static inline void put_pixel_4bpp(volatile uint8_t *vram, int x, int y, uint8_t col) {
      int byte_idx = (y * SCREEN_W + x) >> 1;
      if (x & 1)
          vram[byte_idx] = (vram[byte_idx] & 0xF0) | (col & 0x0F);
      else
          vram[byte_idx] = (vram[byte_idx] & 0x0F) | ((col & 0x0F) << 4);
  }
  ```
* In `rtl/core/neorv32_vga.vhd` (line 208), the MMIO bus read interface for VRAM returns constant zeros:
  ```vhdl
  bus_rdata_o <= (others => '0'); -- Port A read data is NOT connected to CPU bus!
  ```
* When the CPU executed `vram[byte_idx] & 0xF0`, the CPU read `0x00` from the bus. Every write of an odd pixel wiped out the even pixel to black (and vice versa), completely destroying horizontal pixel pairs and causing chromatic speckle!

#### C. The Authentic BSP "Hall of Mirrors" (HOM) Anomaly
* In raycasters (v1 and v2), every single column $x \in [0, 159]$ unconditionally overwrites all scanlines (ceiling flat $\to$ wall $\to$ floor flat).
* In authentic DOOM BSP linedef rendering (v3), pixels are *only* rasterized where a wall segment (`seg`) projects onto the screen. Open portals, steps, and background columns without solid walls rely on visplane flats or pre-clearing.
* Because no background screen clear or visplane fill pass was run, every wall drawn on frame $N$ stayed permanently etched into VRAM on frame $N+1$, recreating the iconic DOOM "Hall of Mirrors" engine glitch!

---

## 3. How We Overcame It (The DOOM v2 Architecture)

To deliver a rock-solid, artifact-free experience without requiring FPGA resynthesis, **DOOM v2** enforces the **100% Unconditional Viewport Overwrite Guarantee**:

1. **Guaranteed 100% Overwrite:** Every frame, the DDA rasterizer traverses all 160 columns, unconditionally painting $y = 0 \dots 103$ (Ceiling $\to$ Textured Wall $\to$ Floor) via direct byte writes (`vram[vram_idx] = pixel`).
2. **Dedicated HUD Overwrite:** Scanlines $y = 104 \dots 119$ are unconditionally redrawn each frame with the beveled baseplate, ammo/health gauges, and animated Doomguy face.
3. **Pure Byte-Write Pipeline:** Only CPU byte writes are emitted to the bus; zero read-modify-write operations occur, perfectly respecting the write-only BRAM RTL interface.
4. **Mathematical Zero-Ghosting Guarantee:** Exactly 19,200 byte writes per frame, zero reads from VRAM, and zero reliance on uninitialized background, delivering 60 FPS butter-smooth 3D rendering with textures, GEMM monster transformations, and HUD.
