# GEMMrv32 DOOM v2: Enhanced 3D Engine with Textures, Sprites & HUD

**Target Platform:** Digilent Basys 3 FPGA (`xc7a35tcpg236-1`)  
**Core Architecture:** NEORV32 RV32IM @ 50.0 MHz + 8x8 Systolic GEMM Coprocessor  
**Video Output:** 640x480 @ 60 Hz 12-bit Color (160x120 Internal Framebuffer with 4x Hardware Upscaling)  
**Binary Image:** `demo_vga_doom_v2.bin` (17,416 bytes, Checksum `0xD98CA47B`)

---

## 1. Executive Summary & Major Upgrades Over v1

`demo_vga_doom_v2` is a major architectural evolution of the bare-metal RISC-V 3D graphics engine. While v1 provided flat-shaded walls, v2 delivers an **authentic retro DOOM experience** featuring textured walls, dynamic billboarded sprites, first-person weapon animation, and the iconic DOOM status bar HUD.

```
+-------------------------------------------------------------------------+
|                  640x480 (160x120 Internal) Frame Layout                |
|                                                                         |
|   +-----------------------------------------------------------------+   |
|   |  3D World Viewport (160 x 104 pixels)                          |   |
|   |  - Authentically Textured Walls (32x32 Tech, Stone, Comp, Rust) |   |
|   |  - 1D Z-Buffer (160 column depth buffer)                        |   |
|   |  - 2.5D Billboarded Sprites (Imps, Explosive Barrels, Medkits) |   |
|   |  - First-Person Weapon (Bobbing Shotgun with Muzzle Flash)      |   |
|   +-----------------------------------------------------------------+   |
|   |  Iconic DOOM Status Bar (160 x 16 pixels)                       |   |
|   |  [ AMMO GAUGE ]       [  DOOMGUY FACE  ]       [ HEALTH GAUGE ] |   |
|   +-----------------------------------------------------------------+   |
+-------------------------------------------------------------------------+
```

---

## 2. Core Engine Architecture

### 2.1 Texture Mapping & Fractional Ray Sampling
Instead of rendering solid colors, v2 computes the exact fractional hit position $\text{wallX}$ along the wall tile:
$$\text{wallX} = \begin{cases} \text{posY} + \text{perpWallDist} \times \text{rayDirY} & (\text{side} = 0) \\ \text{posX} + \text{perpWallDist} \times \text{rayDirX} & (\text{side} = 1) \end{cases}$$
The horizontal texture column $\text{texX} \in [0, 31]$ is extracted from the fractional part of $\text{wallX}$:
$$\text{texX} = \left\lfloor (\text{wallX} \pmod 1) \times 32 \right\rfloor$$
Each vertical pixel in the slice scales along $\text{texY} \in [0, 31]$:
$$\text{texY} = \left\lfloor \frac{(y \times 256 - \text{VIEWPORT\_H} \times 128 + \text{lineHeight} \times 128) \times 32}{\text{lineHeight} \times 256} \right\rfloor$$

### 2.2 1D Depth Buffer (`ZBuffer[160]`)
Before drawing sprites, the raycaster records the perpendicular distance to the closest wall in an array:
$$\text{ZBuffer}[x] = \text{perpWallDist}$$
When sprites are rendered, every vertical stripe of the sprite is tested against $\text{ZBuffer}[x]$. If a wall is closer than the sprite, that column of the sprite is automatically occluded, allowing enemies and barrels to hide behind corners realistically.

### 2.3 The 8x8 Systolic GEMM Coprocessor in Action
Dynamic world entities (Imps, Barrels, Medkits) are represented by 3D coordinates relative to the player. The 8x8 systolic array hardware coprocessor (`0xFFEA0000`) computes the camera-space projection for all 8 entities in a single systolic burst:

$$\begin{bmatrix} X_{cam} \\ Y_{cam} \\ Z_{cam} \\ 1 \end{bmatrix} = \begin{bmatrix} \cos\theta & -\sin\theta & 0 & 0 \\ \sin\theta & \cos\theta & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix} \times \begin{bmatrix} X_{world} - X_{player} \\ Y_{world} - Y_{player} \\ Z_{world} \\ 1 \end{bmatrix}$$

* **Execution Time:** The hardware coprocessor executes this 4x4 matrix multiplication for 8 vectors in parallel using its 256-bit wide DMA channel in under **85 clock cycles** ($66\times$ faster than software RV32IM arithmetic).

### 2.4 First-Person Shotgun & Muzzle Blast
* **Idle State:** Twin gun barrels, wooden forend, and Doomguy's combat gloves are centered at the bottom of the viewport (`x = 64..95`, `y = 80..103`).
* **Walking Bob:** The weapon gently sways up and down during player movement.
* **Firing Action (`SPACE` / `F`):** Depletes 2 ammo shells, triggers a 2-frame fiery yellow/orange muzzle blast, and switches the Doomguy status bar face to an evil grin!

### 2.5 Status Bar HUD (Scanlines 104..119)
* **Left:** Ammo meter (reflects 50 shotgun shells).
* **Center:** Animated Doomguy face (`16x12` pixels) with brown military hair, flesh tones, and blue eyes.
* **Right:** Red health meter (starts at 100%).

---

## 3. Memory & Resource Budget

From the RISC-V GCC build report for `demo_vga_doom_v2`:

```
"Memory utilization:"
   text      data     bss     dec     hex   filename
  17372        28    7584   24984    6198   main.elf
```

| Memory Region | Physical Size | Used by DOOM v2 | Free Space | % Used |
|---|---|---|---|---|
| **Instruction Memory (IMEM)** | 64 KB (65,536 B) | **17,372 B** | **~47.1 KB Free** | **26.5%** |
| **Data Memory (DMEM)** | 64 KB (65,536 B) | **7,612 B** | **~56.3 KB Free** | **11.6%** |
| **Total Memory** | **128 KB** | **24.9 KB** | **~103.4 KB Free** | **19.5%** |

> [!NOTE]
> Even with 4 high-resolution $32 \times 32$ wall textures, 3 dynamic sprite sheets, weapon overlays, HUD graphics, and the full DDA raycaster, **less than 20% of the NEORV32 internal SRAM is utilized**!

---

## 4. Controls & Input Mapping

Dual-mode input is active across both the **Host PC Keyboard** (via USB-UART @ 19200 baud) and the **Basys 3 PS/2 Port (`J2`)**:

| Key | Action | Engine Function |
| :---: | :---: | :--- |
| **`W`** | Move Forward | Steps forward along viewing vector with wall collision check + weapon bob |
| **`S`** | Move Backward | Steps backward with wall collision check + weapon bob |
| **`A`** | Turn Left | Rotates camera direction and plane by $-6.8^\circ$ |
| **`D`** | Turn Right | Rotates camera direction and plane by $+6.8^\circ$ |
| **`SPACE` / `F`** | **Fire Shotgun!** | Fires twin barrels, produces muzzle flash, spends ammo, and makes Doomguy grin |
| **`M`** | Toggle Minimap | Shows / hides the 32x32 top-left radar overlay |
| **`R`** | Respawn / Reset | Resets player position to spawn point `(7.5, 9.5)` and restores ammo/health |

---

## 5. File Directory

```
demo_vga_doom_v2/
├── README.md               # This technical manual
├── main.c                  # Core raycaster, GEMM sprite engine & game loop (25 KB)
├── doom_assets.h           # Header for textures, sprites, weapon & HUD arrays
├── doom_assets.c           # Procedural generator for 32x32 textures & sprites
├── makefile                # Bare-metal RV32IM build rules
├── demo_vga_doom_v2.bin    # 16-byte bootloader packed binary (17.4 KB)
├── main.elf                # Compiled RISC-V ELF
├── gemm_lib/
│   ├── gemmrv_gpu.h        # 2D/3D GPU driver & VRAM API header
│   ├── gemmrv_gpu.c        # Systolic vertex batch transformation routines
│   ├── gemmrv_hw.h         # Direct CFS systolic register definitions
│   └── gemmrv_hw.c         # Hardware tile trigger and DMA sync drivers
```

---

## 6. How to Flash & Run

### Step 1: Disconnect Serial Terminal
Ensure any serial terminal currently using `COM10` (e.g. PuTTY, Arduino monitor) is disconnected so the programming utility can access the port.

### Step 2: Soft Reset to Bootloader
Press **`btnC`** (center button on the Basys 3 D-pad) to reset NEORV32 into the bootloader prompt (`CMD:>`). *(Do NOT press the red PROG button)*.

### Step 3: Flash the Binary
* **Via GEMMrvStudio GUI (`gemm_dashboard.py`):**
  1. Open `gemm_dashboard.py`.
  2. Select **`COM10` @ 19200 baud**.
  3. Select `demo_vga_doom_v2.bin` from `example/bin`.
  4. Click **Program Board (PROG Mode)**.

* **Via PowerShell CLI:**
  ```powershell
  python .\gemm_dashboard.py --flash .\example\bin\demo_vga_doom_v2.bin --port COM10
  ```

### Step 4: Play!
Re-open your serial connection on `COM10` (19200 baud) and enjoy textured walls, animated imps/barrels, and blasting the shotgun with `SPACE`!
