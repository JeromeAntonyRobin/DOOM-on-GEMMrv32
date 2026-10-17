# Comprehensive Chronicle of DOOM Engine Glitches, Root Causes, and Architectural Patches on GEMMrv32

## 1. Executive Summary & Hardware Context

The **GEMMrv32 DOOM Engine** is an authentic, bare-metal 2.5D first-person shooter engine built for the **GEMMrv32 SoC**, a custom FPGA architecture combining a 100 MHz **NEORV32 RISC-V (RV32IM_Zicsr_Zifencei)** processor with a dedicated **GEMM (General Matrix Multiply) Hardware Coprocessor**. The engine targets a native **320x200 @ 8bpp Mode 13h VGA framebuffer** (64,000 bytes) driven by a dual-port Block RAM (BRAM) video controller with a 256-color 12-bit RGB palette DAC.

### Critical Hardware Constraints
- **Strict 32 KB ROM Budget**: The processor's internal instruction memory (IMEM) is mapped to eight Xilinx 7-Series `RAMB36E1` block RAMs configured as **32,768 bytes (32 KB)**. All engine executable code (`.text`) and static read-only data (`.rodata`), including the 256-color PLAYPAL table, textures, sprites, sound stubs, map geometry, and BSP tables, must strictly reside under **32,768 bytes**.
- **Native 320x200 Framebuffer**: Video RAM is mapped at `0x80000000` (`VGA_VRAM_BASE`). The top 168 scanlines ($320 \times 168 = 53,760$ bytes) comprise the 3D viewport, while the bottom 32 scanlines ($320 \times 32 = 10,240$ bytes) display the authentic DOOM 2 status bar HUD.
- **Hardware Acceleration**: 3D monster transformations, view-matrix rotations, and projectile projections are hardware-accelerated via the GEMM coprocessor via memory-mapped coprocessor registers.

---

## 2. Complete Chronicle of Glitches, Root Causes, and Architectural Patches

### Glitch 1: Dual Split-Frame Rendering on Top Half
- **Observed Symptom**: The 3D world rendered as two squeezed, identical sub-frames side-by-side across the upper 100 scanlines, with flickering horizontal tearing and black bars.
- **Root Cause**: During early graphics testing, a 160x100 test stride remained active in the horizontal DDA column stepping, while the VGA BRAM address generator was configured for a 320-pixel stride. Columns were indexed using `(y >> 1) * 320 + (x >> 1)`, causing every scanline to duplicate across half the display width.
- **Architectural Patch**:
  - Implemented unified, full-resolution 320x168 column rasterization.
  - Replaced sub-frame offsets with direct linear framebuffer addressing:
    $$\text{vram\_offset} = y \times \text{SCREEN\_W} + x = y \times 320 + x$$
  - Anchored viewport bounds to $y \in [0, 167]$ and status bar bounds to $y \in [168, 199]$.

---

### Glitch 2: Frame Retention, Buffer Stalls & Persistent Motion Ghosting
- **Observed Symptom**: When moving or turning, walls and ceilings left long horizontal streaks and visual trails. Dead monsters and previous wall positions lingered permanently on the display.
- **Root Cause**: Because clearing a 64,000-byte BRAM over the 32-bit Wishbone bus every frame would take ~160,000 clock cycles (dropping frame rate from 30 FPS to <12 FPS), the engine relied on the 3D raycaster to overwrite previous pixels. However, open sky voids and portal apertures had unrendered columns where neither a ceiling nor a floor flat was written, leaving stale pixels in VRAM indefinitely.
- **Architectural Patch**:
  - Engineered `R_FillBackdrop()` in [`r_segs.c`](file:///C:/Users/Lenovo/Desktop/DOOM-on-GEMMrv32/src/v4/r_segs.c).
  - Maintained per-column `ceilingclip[x]` and `floorclip[x]` buffers during wall rendering.
  - After all front-to-back wall segments are rasterized, `R_FillBackdrop()` sweeps all 320 columns. Any span between `ceilingclip[x] + 1` and `floorclip[x] - 1` that was not occluded by geometry is unconditionally overwritten with its authentic ceiling flat or floor flat.
  - This guaranteed **100% pixel coverage** across all 53,760 viewport pixels every frame without requiring a costly full-screen memory clear.

---

### Glitch 3: Weapon Sprite Flickering & Foreground Wall Occlusion
- **Observed Symptom**: The first-person Super Shotgun sprite flickered aggressively, and nearby walls, door frames, or stairs frequently rendered on top of the gun, erasing the barrels.
- **Root Cause**: The engine rendered the 3D world first and then attempted to overlay the weapon. However, column clipping arrays (`floorclip[x]`) extended all the way to scanline 168, allowing low walls and floors to rasterize over the weapon's designated screen region. When the weapon was drawn over it, any depth checks or buffer flushes caused raster contention and flicker.
- **Architectural Patch**:
  - Introduced the **Precomputed Weapon Top Silhouette Clip** (`weapon_top_clip[SCREEN_W]`).
  - At level start, `R_InitWeaponClip()` samples the exact 1-bit alpha mask of the Super Shotgun patch (`patch_sht2a0`) and determines the topmost opaque scanline for each column $x \in [0, 319]$.
  - In `R_ClearDrawSegs()`, `floorclip[x]` is initialized not to 168, but to `weapon_top_clip[x]`.
  - As a result, the 3D world renderer **stops immediately at the weapon's exact outline**, never drawing a single pixel behind or over the gun.
  - The weapon sprite is then drawn directly into its reserved aperture without overdraw, eliminating 100% of weapon flickering and depth inversion artifacts.

---

### Glitch 4: 64-Bit Software Division Bloat (`__divdi3` ROM Overflow)
- **Observed Symptom**: Adding texture mapping, distant portal projection, and mobjs caused the compiled binary (`main.elf`) to suddenly explode from 30,784 bytes to 34,256 bytes, exceeding the 32,768-byte FPGA BRAM limit by 1,488 bytes and preventing hardware synthesis and flashing.
- **Root Cause**: RV32IM hardware implements 32-bit division (`div`, `rem`), but does NOT support 64-bit integer division. In `R_StoreWallRange` and `R_AddLine`, expressions using `(int64_t)a * b / c` or `(int64_t)dx * wall_len / span_w` silently forced the GCC compiler to link the software division helper `__divdi3` and related 64-bit routines from `libgcc.a`. This single software emulation library consumed **3,472 bytes of ROM**.
- **Architectural Patch**:
  - Restructured all fixed-point texture and projection formulas to strictly fit within 32-bit signed arithmetic ($[-2^{31}, 2^{31}-1]$).
  - Grouped intermediate factors: since screen span is $\le 320$ and wall lengths are $\le 1024$, intermediate products fit safely in 32-bit signed integers ($320 \times 1024 = 327,680 \ll 2.14 \times 10^9$).
  - Eliminated all 64-bit divisions across the entire codebase.
  - Reclaimed **3,472 bytes of ROM**, instantly dropping the binary size back down to 30,784 bytes.

---

### Glitch 5: Peripheral Wall Curvature / Bowing / Hyperbolic Warping
- **Observed Symptom**: When looking at walls obliquely or turning the camera, straight walls appeared to bend like rubber sheets or hyperbolic curves, especially in peripheral vision.
- **Root Cause**: In perspective projection, screen $x$ is proportional to $X_{cam} / Z_{cam}$. Therefore, depth $Z(x)$ across screen columns does NOT vary linearly with $x$; instead, reciprocal depth $1/Z(x)$ is linear in $x$. The engine was linearly interpolating distance $tz$ across screen $x$:
  $$tz(x) = tz_1 + \frac{tz_2 - tz_1}{x_2 - x_1}(x - x_1)$$
  and then computing column height:
  $$y(x) = Y_{center} - \frac{H \times F}{tz(x)}$$
  Because $y(x) \propto \frac{1}{A x + B}$, a planar vertical wall was being projected onto screen space as a **hyperbola**! At oblique angles where $tz_1 = 10$ and $tz_2 = 200$, the center of the wall suffered a 550% height error, sagging dramatically into a curve.
- **Architectural Patch**:
  - Applied the fundamental theorem of projective geometry: **any straight 3D line under planar perspective projection maps to a straight 2D line on screen**.
  - In [`R_StoreWallRange`](file:///C:/Users/Lenovo/Desktop/DOOM-on-GEMMrv32/src/v4/r_segs.c), boundary heights (`raw_y_ceil1, raw_y_ceil2`, `raw_y_floor1, raw_y_floor2`, and step portals `raw_b_ceil`, `raw_b_floor`) are computed once at the segment endpoints:
    $$\text{raw\_y\_ceil1} = Y_c - \frac{(H_{ceil} - viewz) \times F}{tz_1}, \quad \text{raw\_y\_ceil2} = Y_c - \frac{(H_{ceil} - viewz) \times F}{tz_2}$$
  - For each column $x$, heights are interpolated linearly across screen space:
    $$\text{raw\_y\_ceil}(x) = \text{raw\_y\_ceil1} + \frac{\text{raw\_y\_ceil2} - \text{raw\_y\_ceil1}}{x_2 - x_1}(x - x_1)$$
  - Eliminated all hyperbolic warping; straight walls remain straight lines across the viewport.
  - Removed two 32-bit hardware divisions per column from the rasterizer loop, accelerating rendering.

---

### Glitch 6: Running Through Walls & See-Through Wall Glitch
- **Observed Symptom**: The player could walk straight through solid one-sided walls. Furthermore, when approaching a wall closely, the wall suddenly vanished and the void behind the level was revealed.
- **Root Cause**:
  1. The player movement handler in `main.c` simply incremented `player_mobj->x` and `player_mobj->y` without testing against linedef geometry.
  2. The near-plane clipping threshold was set to $tz = 4$. When the camera approached within 4 units of a wall, the line endpoints were clipped or inverted, culling the wall and exposing the unrendered void.
- **Architectural Patch**:
  - Implemented 2D line-segment collision detection in [`P_CheckPosition(px, py)`](file:///C:/Users/Lenovo/Desktop/DOOM-on-GEMMrv32/src/v4/doom_map.c).
  - For each blocking line (one-sided walls and two-sided portals with step height $>32$ units), the engine calculates the orthogonal distance from the player's candidate coordinate $(p_x, p_y)$ to the line segment $(v_1, v_2)$ using projected dot products.
  - Enforced a player collision radius of **$R = 18$ units** ($R^2 = 324$).
  - Implemented **axial wall sliding**: if moving along $(dx, dy)$ collides with a wall, the engine tests moving along $dx$ only, then $dy$ only, allowing the player to glide smoothly along walls.
  - Because the player's radius is 18 units, the camera eye is physically prevented from coming closer than 18 units to any solid wall. Since $18 > 4$, the near-plane clipping threshold is never breached, completely eliminating the see-through wall glitch.

---

### Glitch 7: Missing Floor Elevation & Floating over Courtyard / Stairs
- **Observed Symptom**: The player remained pinned at $z = 0$. When ascending the stairs, the steps rose above the player's eye level; when walking into the sunken toxic courtyard or canals, the player hovered in mid-air 32 units above the slime.
- **Root Cause**: The player mobj had no sector floor tracking logic; `player_mobj->z` was initialized to 0 and never updated.
- **Architectural Patch**:
  - Engineered [`P_GetSectorFloorAt(px, py)`](file:///C:/Users/Lenovo/Desktop/DOOM-on-GEMMrv32/src/v4/doom_map.c) to determine the exact floor height of the sector containing $(px, py)$ across both levels.
  - Added smooth vertical elevation and gravity interpolation to [`render_doom_v3_frame()`](file:///C:/Users/Lenovo/Desktop/DOOM-on-GEMMrv32/src/v4/main.c):
    - When stepping up onto higher sectors (stairs, ledges), the player steps up smoothly at $+4$ units/frame.
    - When stepping off ledges into sunken canals or toxic pits, gravity pulls the player down at $-8$ units/frame until reaching the floor.
  - Camera `viewz` is dynamically calculated as:
    $$viewz = player\_mobj \to z + \text{TO\_FIXED}(41)$$
  - Looking at monsters from raised ledges or sunken pits produces authentic 3D vertical parallax.

---

### Glitch 8: ROM Bloat & Dead Code Optimization
- **Observed Symptom**: Adding MAP02 and collision detection caused the binary size to exceed 32 KB by 464 bytes (`section .rodata will not fit in region rom`).
- **Root Cause**:
  1. The original map definition included 50 `side_t` sidedef structs that were never read by the rasterizer (consuming 800 bytes).
  2. `line_t` included an unused `sidenum[2]` pointer pair (consuming 336 bytes).
  3. The status bar included two full 24x30 face patches (`patch_stfevl0` and `patch_stfst00`), where `patch_stfevl0` alone consumed 816 bytes for a brief 3-frame grimace.
- **Architectural Patch**:
  - Removed `sidenum[2]` and `map_sidedefs` completely, saving **1,136 bytes**.
  - Unified the status bar face to `patch_stfst00`, eliminating `raw_stfevl0` and saving **816 bytes**.
  - Total reclaimed ROM: **1,952 bytes**.
  - The final binary with two complete levels, collision detection, elevation tracking, HW/SW toggle, and live FPS counter compiled to **32,380 bytes** (388 bytes below the 32 KB ceiling).

---

## 3. Dual-Engine Rendering Pipeline: HW vs. SW Modes

To benchmark and showcase the acceleration provided by the GEMM hardware coprocessor, the engine incorporates runtime-switchable rendering paths:

### 1. Hardware-Accelerated Mode (`[HW]`)
- Dynamic 2.5D mobjs (Imps, Demons, Shotgun Guys, Barrels) have their 3D coordinates packed into an 8-vertex aligned buffer.
- The view rotation matrix is loaded into coprocessor registers.
- The GEMM coprocessor performs batch matrix transformation in hardware via `gpu_transform_vertices_8x()`.
- Displayed in bright emerald green (`[HW]`) on the HUD.

### 2. Software Fallback Mode (`[SW]`)
- When switched to SW mode (hotkey `H` or `T`), hardware coprocessor calls are bypassed.
- Transformations are computed in software on the RV32IM CPU core via fixed-point integer math.
- Displayed in blood red (`[SW]`) on the HUD.

### 3. Real-Time FPS Counter
- The engine reads the hardware cycle counter CSR (`mcycle`) once per frame.
- Elapsed cycles are compared against the processor clock frequency from the NEORV32 System Information register:
  $$f_{clk} = \text{NEORV32\_SYSINFO}\to\text{CLK} = 100,000,000 \text{ Hz}$$
- FPS is recalculated every 500 ms using pure 32-bit arithmetic:
  $$\text{display\_fps} = \frac{\text{frame\_counter} \times (f_{clk} / 1000)}{\text{elapsed\_cycles} / 1000}$$
- Rendered continuously in the top-right corner of the 3D viewport (`XX FPS [HW]` / `XX FPS [SW]`).

---

## 4. Multi-Map Architecture

The engine supports multiple authentic levels switchable instantly at runtime:

| Map ID | Name | Theme | Sectors | Lines | Features |
|---|---|---|---|---|---|
| **0** | **E1M1** | Starport Hangar & Toxic Courtyard | 4 | 22 | Elevated stairs (+32), Sunken toxic nukage pool (-32), Computer room (+16), Armor pedestal (+32) |
| **1** | **MAP02** | Underhalls & Toxic Canals | 4 | 20 | Central walkway hub (0), Sunken slime canal (-32), Raised sniper battlement (+48), Armory bunker (+16), Canal pillar |

---

## 5. Control Keymap Reference

| Key | Action |
|---|---|
| **W** / **S** | Move Forward / Backward (with solid wall collision & axial sliding) |
| **A** / **D** | Turn Camera Left / Right (64-entry fixed-point trigonometric LUT) |
| **SPACE** / **F** | Fire Super Shotgun (20 buckshot pellets with animated muzzle flash) |
| **H** / **T** | Toggle Hardware Acceleration (`[HW]` GEMM Coprocessor vs `[SW]` CPU) |
| **1** | Load Level 1: **E1M1 HANGAR** |
| **2** | Load Level 2: **MAP02 CANALS** |
| **N** | Cycle to Next Map |
| **M** | Toggle 2D Real-Time Minimap Overlay |
| **R** | Respawn / Reset Current Level |
| **B** / **Q** | Return to NEORV32 Bootloader prompt |
