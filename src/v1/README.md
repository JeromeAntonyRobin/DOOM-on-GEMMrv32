# GEMMrv32 DOOM v1: 3D DDA Raycaster & Systolic Coprocessor Engine

**Target Platform:** Digilent Basys 3 FPGA (`xc7a35tcpg236-1`)  
**Core Architecture:** NEORV32 RV32IM @ 50.0 MHz + 8x8 Systolic GEMM Coprocessor  
**Video Output:** 640x480 @ 60 Hz 12-bit Color (160x120 Internal Framebuffer with 4x Hardware Upscaling)

---

## 1. Overview & Capabilities

`demo_vga_doom_v1` is the foundation release of the 3D DOOM graphics engine running bare-metal on the **GEMMrv32 SoC**. It transforms the Digilent Basys 3 FPGA into a fully autonomous, real-time 3D workstation without requiring any operating system or external graphics cards.

### Core Engine Features:
* **Digital Differential Analysis (DDA) 3D Raycaster**: Casts 160 horizontal rays through a discrete $16 \times 16$ tile map.
* **Fisheye Distortion Elimination**: Calculates true perpendicular wall distance ($\text{perpWallDist}$) rather than Euclidean distance to maintain straight horizontal wall lines.
* **Single-Pass Column Rasterization**: Draws each column from top to bottom (`Ceiling -> Wall -> Floor`) in a single vertical pass using 8-bit memory stores, guaranteeing 100% pixel coverage and eliminating ghosting.
* **Dynamic Distance & Directional Shading**: 10 depth bands per wall type, plus differential shading between North-South and East-West wall faces for true 3D visual depth.
* **8x8 Systolic Coprocessor Integration**: Offloads world-space to camera-space coordinate matrix transformations for all 8 dynamic entities directly to the Custom Functions Subsystem (CFS) hardware array ($66\times$ faster than software RV32IM execution).
* **32x32 Real-Time Minimap Radar**: Displays the maze layout, player coordinate, and directional viewing frustum in the top-left corner (toggleable with `M`).
* **Fixed-Point Trigonometry**: 16.16 fixed-point arithmetic for sub-pixel player position, forward/strafe vectors, and camera plane rotation ($5^\circ$ turn rate per step).
* **Wall Collision Detection**: Bounding grid-cell checks prevent the player from clipping through walls.

---

## 2. The Basys 3 RAM Budget Explained

A frequent question is: **Why is the system RAM budgeted at 64 KB IMEM and 64 KB DMEM?**

### The Physical Reality of the Artix-7 35T FPGA
1. **Zero External RAM on the Basys 3**:
   Unlike higher-end boards (such as the Nexys A7 with 128 MB DDR2 or Arty A7 with 256 MB DDR3), the Digilent Basys 3 has **no external RAM chip** (no SDRAM, DDR, or SRAM).
2. **On-Chip Silicon Limits**:
   All memory in our entire computer system must be synthesized out of the FPGA's internal **Block RAM (BRAM)** primitives.
   * The Xilinx Artix-7 `XC7A35T` has exactly **50 Block RAMs** (each 36 Kbits = 4.5 KB).
   * **Total physical on-chip SRAM on the entire chip: $50 \times 4.5 \text{ KB} = 225 \text{ Kilobytes}$. That is the absolute hardware ceiling.**

### How the 225 KB BRAM is Allocated:
| Subsystem | RAM Allocated | Physical BRAM36s | Purpose |
|---|---|---|---|
| **Instruction Memory (IMEM)** | **64 KB** | 16 BRAMs | Stores the compiled RV32IM machine code instructions |
| **Data Memory (DMEM)** | **64 KB** | 16 BRAMs | Global variables, static data, call stack, and heap |
| **VGA Video RAM (VRAM)** | **19.2 KB** | 5 BRAMs | 160x120 8bpp dual-port framebuffer (Port A: CPU, Port B: VGA) |
| **GEMM Wide-DMA BRAM** | **32 KB** | 8 BRAMs | 256-bit wide DMA scratchpad buffer for systolic array feeding |
| **CLUT & Peripherals** | **~4 KB** | Distributed / 1 | 256x12-bit palette table + UART & PS/2 FIFOs |
| **Total Allocation** | **~183.2 KB / 225 KB** | **46 / 50 BRAMs (~92%)** | **Near maximum silicon utilization!** |

Attempting to allocate 128 KB for DMEM would require 32 BRAMs just for data, pushing total utilization past 50 BRAMs and causing Vivado synthesis to fail with `ERROR: [Place 30-484] Not enough BRAM blocks on xc7a35t`. The 64 KB IMEM + 64 KB DMEM split is the optimal configuration for maximum performance and stability.

---

## 3. Memory Map

| Address Range | Size | Subsystem / Device | Access |
|---|---|---|---|
| `0x00000000 - 0x0000FFFF` | 64 KB | Instruction Memory (IMEM) | Read / Execute |
| `0x80000000 - 0x8000FFFF` | 64 KB | Data Memory (DMEM) | Read / Write |
| `0x90000000 - 0x90004B00` | 19,200 B | VGA Dual-Port Framebuffer (VRAM) | Byte Read / Write |
| `0x90010000 - 0x900103FF` | 1,024 B | VGA 256-entry 12-bit Palette (CLUT) | Word Read / Write |
| `0x90010400` | 4 B | VGA Status Register (`bit[0]` = in_vblank) | Read-Only |
| `0x90020000` | 4 B | PS/2 Keyboard Data Register (Scancode FIFO) | Read-Only |
| `0x90020004` | 4 B | PS/2 Keyboard Status Register (`bit[0]` = ready) | Read-Only |
| `0xFFEA0000 - 0xFFEA001C` | 32 B | GEMM Systolic Accelerator Registers (CFS) | Read / Write |
| `0xFFFFFFE0 - 0xFFFFFFFF` | 32 B | NEORV32 UART0 Primary Serial Interface | Read / Write |

---

## 4. Controls & Input Mapping

The engine accepts dual-mode input simultaneously from both the **Host PC Keyboard** (via USB-UART @ 19200 baud) and the **Onboard PS/2 Port** (`J2`):

| Key | Action | Engine Function |
| :---: | :---: | :--- |
| **`W`** | Move Forward | Advances along player direction vector with collision check |
| **`S`** | Move Backward | Retreats along direction vector with collision check |
| **`A`** | Turn Left | Rotates camera direction and plane by $-5^\circ$ using fixed-point matrix |
| **`D`** | Turn Right | Rotates camera direction and plane by $+5^\circ$ using fixed-point matrix |
| **`M`** | Toggle Minimap | Shows / hides the 32x32 top-left overhead radar overlay |
| **`R`** | Respawn / Reset | Resets player position to spawn point `(7.5, 9.5)` facing down the corridor |

---

## 5. File Directory

```
demo_vga_doom_v1/
├── README.md               # This architectural reference manual
├── main.c                  # Core DOOM 3D raycaster & game loop (18 KB)
├── makefile                # Bare-metal RV32IM build rules
├── demo_vga_doom.bin       # Ready-to-flash 16-byte header packed bootloader image
├── main.elf                # Compiled RISC-V ELF executable
├── gemm_lib/
│   ├── gemmrv_gpu.h        # 2D/3D GPU driver & VRAM API header
│   ├── gemmrv_gpu.c        # Systolic vertex batch transformation routines
│   ├── gemmrv_hw.h         # Direct CFS systolic register definitions
│   └── gemmrv_hw.c         # Hardware tile trigger and DMA sync drivers
```

---

## 6. How to Build & Flash

### Option A: Using GEMMrvStudio GUI (`gemm_dashboard.py`)
1. Open `gemm_dashboard.py` in Python.
2. Select **`COM10` @ 19200 baud**.
3. In the project explorer, select `demo_vga_doom_v1`.
4. Click **Build Project** (compiles ELF and outputs `demo_vga_doom.bin`).
5. Press **`btnC`** (center button on Basys 3 D-pad) to enter bootloader.
6. Click **Program Board (PROG Mode)**.

### Option B: Command Line (PowerShell)
```powershell
# Set path to Vivado RISC-V GCC toolchain
$env:PATH = "D:\vivado\2025.2\gnu\riscv\nt\bin;D:\vivado\2025.2\Vivado\gnuwin\bin;" + $env:PATH

# Navigate to project directory
cd .\example\src\demo_vga_doom_v1

# Compile ELF
make RISCV_PREFIX=riscv64-unknown-elf- clean elf

# Flash over serial bootloader using GEMMEngine
python -c "
import sys; sys.path.append(r'.')
from gemm_engine import GEMMEngine
engine = GEMMEngine(port='COM10', baud=19200)
success, info = engine.flash_binary(r'.\example\bin\demo_vga_doom_v1.bin', log_cb=print)
print('Flashed:', success, info)
"
```
