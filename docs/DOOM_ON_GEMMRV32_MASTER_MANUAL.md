# DOOM-on-GEMMrv32: Comprehensive Architecture, Driver, and Engineering Manual

```
Project:            DOOM-on-GEMMrv32: Bare-Metal Authentic DOOM Engine on FPGA
Target Silicon:     Xilinx Artix-7 (XC7A35T-1CPG236C) on Digilent Basys 3
Processor Core:     NEORV32 RV32IM CPU @ 50.0 MHz (Zicsr, Zifencei)
Vector Unit:        8x8 Systolic-Array GEMM Coprocessor (CFS @ 0xFFEA0000)
Video Display:      Dual-Port VGA Controller (160x120 @ 8bpp upscaled 4x to 640x480 @ 60 Hz)
Repository Path:    /home/econsystems/random/DOOM-on-GEMMrv32
```

---

## 1. Executive Summary & System Vision

**DOOM-on-GEMMrv32** transforms the Digilent Basys 3 FPGA into a self-contained, bare-metal 3D workstation executing an authentic Id Software DOOM engine. Rather than relying on an operating system, software emulator, or external DRAM chip, the entire game engine, BSP tree renderer, monster AI, 256-color palette, and hardware drivers run directly on a 50 MHz NEORV32 RISC-V soft processor backed by a custom **8x8 Systolic GEMM Coprocessor** and a custom **Dual-Port VGA Framebuffer Core**.

```
+-----------------------------------------------------------------------------------+
|                        BASYS 3 STANDALONE FPGA WORKSTATION                        |
|                                                                                   |
|   +---------------------------------------------------------------------------+   |
|   |                        NEORV32 RV32IM CPU @ 50.0 MHz                      |   |
|   |            32-64 KB Instruction BRAM  |  64-96 KB Data BRAM               |   |
|   +---------------------------------------------------------------------------+   |
|          │                                                       │                |
|          │ CFS Coprocessor Port (0xFFEA0000)                     │ Wishbone / XBUS|
|          ▼                                                       ▼                |
|   +--------------------------+                         +----------------------+   |
|   |  8x8 Systolic GEMM Unit  |                         |  Dual-Port VGA Core  |   |
|   |  - 256-bit Wide DMA      |                         |  - 19,200 B Framebuf |   |
|   |  - 66x Vector Speedup    |                         |  - 256 x 12b Palette |   |
|   |  - 3D Entity Projections |                         |  - 4x HW Scaler 60Hz |   |
|   +--------------------------+                         +----------------------+   |
|                                                                  │                |
|                                                                  ▼                |
|                                                            12-bit RGB DAC         |
|                                                            VGA Monitor (J1)       |
+-----------------------------------------------------------------------------------+
```

---

## 2. Hardware Memory Map & Bus Interfaces

The system bridges internal processor memory and memory-mapped peripheral interfaces across three primary bus layers:

### 2.1 Memory Map Specification

| Memory Region | Physical Address Range | Size | Access Type | Hardware Entity |
|---|---|---|---|---|
| **Instruction RAM (IMEM)** | `0x00000000` – `0x0000FFFF` | 64 KB | Read/Fetch | On-chip Dual-Port BRAM |
| **Data RAM (DMEM)** | `0x80000000` – `0x80017FFF` | 96 KB | Read/Write | On-chip 256-bit Wide BRAM / Crossbar |
| **VGA Framebuffer (VRAM)** | `0x90000000` – `0x90004B00` | 19,200 B | Write-Only | Port A of Dual-Port Video BRAM ($160 \times 120$) |
| **VGA Palette CLUT** | `0x90010000` – `0x900103FF` | 1,024 B | Read/Write | $256 \times 12\text{-bit}$ RGB Lookup Table |
| **VGA Status & Sync** | `0x90010400` | 4 B | Read-Only | Bit 0: `in_vblank`, Bit 1: `pclk_locked` |
| **PS/2 Keyboard Data** | `0x90020000` | 4 B | Read-Only | 8-bit Scancode from 16-byte FIFO |
| **PS/2 Keyboard Status** | `0x90020004` | 4 B | Read-Only | Bit 0: `has_key` (Data Ready) |
| **Systolic GEMM Coprocessor** | `0xFFEA0000` – `0xFFEA001F` | 32 B | Read/Write | CFS Registers `REG[0..7]` |
| **UART0 Console** | `0xFFFFFFE0` | 16 B | Read/Write | NEORV32 Built-in Serial (19200 baud) |

---

## 3. Hardware Modules & Coprocessor Mechanics

### 3.1 8x8 Systolic-Array GEMM Coprocessor (`rtl/core/neorv32_cfs.vhd`)
The coprocessor is integrated through the NEORV32 Custom Functions Subsystem (CFS) at base address `0xFFEA0000`. It executes parallel matrix multiply-accumulate operations:

$$\mathbf{C} = \mathbf{A} \times \mathbf{B}$$

* **Register Definition**:
  - `REG[0]` (`0x00`): Base pointer to Matrix $\mathbf{A}$ (signed 8-bit, 32-byte aligned).
  - `REG[1]` (`0x04`): Base pointer to Matrix $\mathbf{B}$ (signed 8-bit, 32-byte aligned).
  - `REG[2]` (`0x08`): Base pointer to Output Matrix $\mathbf{C}$ (signed 32-bit accumulators).
  - `REG[3]` (`0x0C`): Stride for Matrix $\mathbf{A}$ (must be a multiple of 8).
  - `REG[4]` (`0x10`): Stride for Matrix $\mathbf{B}$ (must be a multiple of 8).
  - `REG[5]` (`0x14`): Stride for Matrix $\mathbf{C}$ (must be a multiple of 8).
  - `REG[6]` (`0x18`): Control/Status register (`bit 0`: `cmd_start`, `bit 1`: `clear_acc`, `bit 2`: `status_done`).
  - `REG[7]` (`0x1C`): Tile dimensions `(tile_M << 16) | (tile_K << 8) | tile_N`.

* **3D Coordinate Projection Vectorization**:
  In `p_mobj.c`, the coprocessor transforms up to 8 dynamic world entities (Imps, Barrels, Projectiles) per batch into camera coordinate space:

$$\begin{bmatrix} X_{cam} \\ Y_{cam} \\ Z_{cam} \\ 1 \end{bmatrix}_i = \begin{bmatrix} \cos\theta & -\sin\theta & 0 & 0 \\ \sin\theta & \cos\theta & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix} \times \begin{bmatrix} X_{world} - X_{view} \\ Y_{world} - Y_{view} \\ Z_{world} - Z_{view} \\ 1 \end{bmatrix}_i$$

The coprocessor evaluates all 8 vector transforms in **under 85 clock cycles**, compared to $\sim 5,600$ clock cycles required by software integer emulation ($66\times$ acceleration).

### 3.2 Dual-Port VGA Raster Core (`rtl/core/neorv32_vga.vhd`)
* **Display Timing**: Generates standard VESA $640 \times 480$ @ 60 Hz timing signals using a 25.0 MHz pixel clock.
* **4x Hardware Nearest-Neighbor Upscaler**: Maps the internal $160 \times 120$ framebuffer coordinates to physical raster scanlines:
  $$x_{fb} = \lfloor x_{vga} / 4 \rfloor = x_{vga} \gg 2, \quad y_{fb} = \lfloor y_{vga} / 4 \rfloor = y_{vga} \gg 2$$
  $$\text{VRAM\_ADDR} = y_{fb} \times 160 + x_{fb}$$
* **Palette Engine (CLUT)**: A 256-entry by 12-bit RGB table (4-bit Red, 4-bit Green, 4-bit Blue) translates 8-bit palette indices into direct analog signals across the resistor ladder DAC.

---

## 4. Software Driver Call Stack & Implementation Rationale

The software architecture is decoupled into distinct abstraction layers:

```
+-------------------------------------------------------------------+
| APPLICATION LAYER: main.c (Game Loop, Input Coordinator, HUD)    |
+-------------------------------------------------------------------+
       │                                             │
       ▼                                             ▼
+-----------------------------+        +----------------------------+
| 3D ENGINE PIPELINE          |        | ENTITY & TICKER SUBSYSTEM  |
| - r_bsp.c (BSP Traversal)   |        | - p_mobj.c (Thinkers/AI)   |
| - r_segs.c (Wall Clipping)  |        | - doom_map.h (Level Lines) |
+-----------------------------+        +----------------------------+
       │                                             │
       ▼                                             ▼
+-------------------------------------------------------------------+
| HARDWARE DRIVER LAYER: gemm_lib                                   |
| - gemmrv_gpu.c: vga_set_palette(), gpu_transform_vertices_8x()    |
| - gemmrv_hw.c:  hw_gemm_tile(), gemmrv_mult()                     |
+-------------------------------------------------------------------+
       │                                             │
       ▼                                             ▼
+-------------------------------------------------------------------+
| REGISTER & MMIO LAYER: neorv32.h                                  |
| - NEORV32_CFS->REG[0..7], VGA_VRAM_BASE, PS2_DATA_REG, UART0     |
+-------------------------------------------------------------------+
```

### 4.1 Low-Level Hardware Tile Driver (`src/gemm_lib/gemmrv_hw.c`)
```c
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
    if(clear) cmd |= 2;
    if(store) cmd |= 4;
    
    asm volatile ("fence" ::: "memory");
    NEORV32_CFS->REG[6] = cmd;
    
    // Busy-wait on hardware completion status bit
    while ((NEORV32_CFS->REG[6] & (1 << 2)) == 0) {}
    
    NEORV32_CFS->REG[6] = 0; // Clear start
    while ((NEORV32_CFS->REG[6] & (1 << 2)) != 0) {} // Wait for idle
    
    asm volatile ("fence" ::: "memory");
}
```
**Rationale:** The `fence` instructions guarantee that compiler reordering and load/store buffers do not emit DMA reads before memory buffers are fully flushed to data RAM.

### 4.2 Front-to-Back BSP Traversal (`src/r_bsp.c`)
Standard raycasters cast individual rays outward into a 2D grid. In contrast, the BSP engine traverses Id Software binary space partition nodes recursively:
1. Determines if camera position $(viewx, viewy)$ is on the right or left child side of the partition line.
2. Traverses the near child subtree first.
3. If screen columns remain unoccluded (`floorclip[x] > ceilingclip[x]`), traverses the far child subtree.
4. Leaves (subsectors) emit linedef segments (`segs`) into `R_AddLine()`.

---

## 5. Critical Technical Anomalies, Root Causes & Mitigations

During the development and testing of this architecture on the Basys 3 FPGA, six major hardware/software failures occurred. Below is the detailed post-mortem and mitigation for each.

---

### Anomaly 1: The "Ghost Wall" Comb Striping Glitch
* **Observed Symptom**: When moving or rotating, previous wall textures remained frozen on the screen, split by vertical 1-pixel transparent gaps every 4 pixels horizontally.
* **Root Cause**:
  In `rtl/core/neorv32_vga.vhd`, the framebuffer BRAM was declared byte-wide. The bus interface steered incoming writes according to the lower address bits `bus_addr_i(1 downto 0)`:
  ```vhdl
  case bus_addr_i(1 downto 0) is
    when "00"   => vram(byte_addr) <= bus_wdata_i(7 downto 0);
    when "01"   => vram(byte_addr) <= bus_wdata_i(15 downto 8);
    when "10"   => vram(byte_addr) <= bus_wdata_i(23 downto 16);
    when others => vram(byte_addr) <= bus_wdata_i(31 downto 24);
  end case;
  ```
  The software screen clear executed 32-bit word stores:
  ```c
  volatile uint32_t *vram32 = (volatile uint32_t *)VGA_VRAM_BASE;
  vram32[i] = clear_word;
  ```
  Because word stores present address bits `bus_addr_i(1 downto 0) = "00"`, only byte lane 0 was written to memory. Byte offsets 1, 2, and 3 were ignored. Thus, **75% of pixel columns were never cleared**, producing comb-like vertical stripes.
* **Mitigation**:
  Enforced a **single-pass vertical column rasterizer** using native 8-bit byte stores (`uint8_t`), writing ceiling, wall, and floor pixels directly to matching memory addresses.

---

### Anomaly 2: The 19.2 KB Hardware Hard Wall & 4bpp Read Trap
* **Observed Symptom**: When attempting native $320 \times 200$ @ 4bpp Mode, the bottom 40% of the display froze completely, and neighboring pixels showed chromatic speckles.
* **Root Cause**:
  1. **Address Overflow**: A $320 \times 200$ buffer requires 32,000 bytes. The synthesized FPGA bitstream allocated 19,200 bytes ($160 \times 120$). Scanline writes beyond $y = 120$ generated addresses $\ge 19,200$, which the hardware write comparator discarded:
     ```vhdl
     if byte_addr < 19200 then
         -- Allow write
     end if;
     ```
  2. **Write-Only Read Trap**: To pack two 4bpp nibbles per byte, the CPU executed:
     ```c
     vram[byte_idx] = (vram[byte_idx] & 0xF0) | (col & 0x0F);
     ```
     In `rtl/core/neorv32_vga.vhd`, Port A read returned hardwired zero (`bus_rdata_o <= (others => '0')`). Reading `vram[byte_idx]` returned `0x00`, inadvertently wiping the adjacent nibble to black on every write.
* **Mitigation**:
  Locked the engine to native **$160 \times 120$ @ 8bpp**, providing full byte-per-pixel access within the 19,200-byte boundary and eliminating all read-modify-write operations.

---

### Anomaly 3: The Hall of Mirrors (HOM) & Portal Floor Stalls
* **Observed Symptom**: Moving through staircases or doorways left frozen weapon trails, stuck muzzle flashes, and psychedelic floor retention across scanlines $y = 117..167$.
* **Root Cause**:
  In authentic DOOM BSP rendering, two-sided portals draw only step faces and adjust `floorclip[x]`. Solid walls behind the portal only draw down to `floorclip[x]` and then set `floorclip[x] = -1`. The floor aperture in front of the step was never drawn by the step, never drawn by the distant wall, and ignored by post-processing, leaving stale frame data intact.
* **Mitigation**:
  Implemented `R_FillBackdrop()`: before masked entities are drawn, any column where `ceilingclip[x] < floorclip[x]` is filled with authentic ceiling flat (index 112) above the horizon and stone floor flat (index 72) below the horizon, guaranteeing **100% pixel coverage every frame**.

---

### Anomaly 4: The Blocking UART Keyboard Lockup
* **Observed Symptom**: The game rendered one frame upon keystroke and then completely stalled; when idle, the frame rate dropped to 0 FPS.
* **Root Cause**:
  In `main.c`:
  ```c
  if (neorv32_uart0_available()) {
      char cmd = (char)neorv32_uart0_getc();
      handle_input(cmd);
  }
  ```
  `neorv32_uart0_available()` checks whether UART hardware is present in the synthesized FPGA SoC (always returns true). `neorv32_uart0_getc()` is a blocking loop waiting for `char_received == 1`. Thus, the CPU halted on every frame until a user pressed a key.
* **Mitigation**:
  Replaced with non-blocking FIFO polling:
  ```c
  if (neorv32_uart0_char_received()) {
      char cmd = neorv32_uart0_char_received_get();
      handle_input(cmd);
  }
  ```

---

### Anomaly 5: Vanishing Walls & Inverted Navigation
* **Observed Symptom**: Rotating 360 degrees caused boundary walls to disappear into voids; turning left with `'A'` swung the camera right, and the 2D minimap rendered South upwards.
* **Root Cause**:
  1. DOOM requires Counter-Clockwise (CCW) perimeter winding ($v_1 \to v_2$) so that the sector interior is on the right side. Clockwise linedef definitions caused one-sided walls to fail the visibility test ($x_1 < x_2$) and get back-face culled.
  2. The input handler decremented the angle index on `'a'`, reversing yaw.
  3. The minimap mapped world $+Y$ downwards ($y = (Y + 200)/16$).
* **Mitigation**:
  Re-defined all 18 map linedefs with strict CCW winding, added dynamic coordinate/sector swapping for two-sided portals viewed from the reverse direction, aligned angle incrementation with standard trigonometry, and mapped world $+Y$ upwards on the HUD.

---

### Anomaly 6: Screen Tearing & BRAM Capacity Bottleneck
* **Observed Symptom**: Rapid turns caused jagged horizontal tear lines across wall columns.
* **Root Cause**:
  The Basys 3 FPGA (`xc7a35t`) has **50 Block RAM tiles (225 KB total)**. 48 tiles are committed to IMEM (32-64 KB), DMEM (64-96 KB), and the active framebuffer (19.2-64 KB). Instantiating a second back-buffer for double buffering requires an additional 64 KB, exceeding physical silicon capacity.
* **Mitigation**:
  Implemented **Beam-Synchronized Rasterization**:
  The CPU polls the hardware vertical blanking bit (`0x90010400`) via `wait_for_vblank()` before rendering. Rendering proceeds top-to-bottom ahead of the raster scanout, avoiding mid-frame collision.

---

## 6. Directory Index & File Map

```
DOOM-on-GEMMrv32/
├── Makefile                                # Top-level open-source build & flash orchestration
├── INSTALLED_TOOLS.txt                     # Tracking registry of local tools & removal instructions
│
├── bit/                                    # FPGA Configuration Bitstreams
│   ├── neorv32_basys3.bit                  # Synthesized Basys 3 bitstream (tracked)
│   ├── neorv32_basys3.bin                  # Macronix SPI flash binary
│   ├── program_jtag.tcl                    # Vivado batch JTAG programmer
│   └── flash_basys3.tcl                    # Vivado batch SPI flash programmer
│
├── bin/                                    # Target Firmware Executables
│   └── demo_vga_doom.bin                   # Bootloader-ready verified DOOM binary
│
├── src/                                    # Engine & Hardware Driver Source Tree
│   ├── main.c                              # Game entry, main loop, HUD, Super Shotgun
│   ├── makefile                            # RISC-V application makefile
│   ├── r_bsp.c / r_bsp.h                   # Front-to-back recursive BSP traversal
│   ├── r_segs.c / r_segs.h                 # Linedef projection, clipping, backdrop fill
│   ├── p_mobj.c / p_mobj.h                 # Entity thinkers, 35Hz ticker, GEMM batch dispatch
│   ├── doom_map.h                          # MAP01 multi-height geometry & spawn records
│   ├── doom_defs.h                         # Core fixed-point arithmetic & data structures
│   ├── doom_assets_authentic.c / .h        # DOOM 2 PLAYPAL, STARTAN3, Super Shotgun, Doomguy
│   │
│   └── gemm_lib/                           # Hardware Drivers
│       ├── gemmrv_hw.h / gemmrv_hw.c       # Low-level CFS 8x8 systolic array registers
│       └── gemmrv_gpu.h / gemmrv_gpu.c     # High-level VGA & 3D vector API
│
├── tools/                                  # Development & Flashing Utilities
│   ├── upload_serial.py                    # Python open-source UART bootloader streamer
│   ├── image_gen.py                        # Executable ELF to bootable image generator
│   └── generate_authentic_assets.py        # WAD lump extractor and palette quantizer
│
├── docs/                                   # Architectural Specifications
│   ├── DOOM_ENGINE_ARCHITECTURE.md         # 3D pipeline & mathematical derivations
│   ├── HARDWARE_SPECIFICATION.md           # Master hardware & memory reference
│   ├── DOOM_GLITCH_ANALYSIS.md             # 19.2 KB boundary & bus steering post-mortem
│   ├── AUTHENTIC_RENDERING_PIPELINE_FIXES.md# Portal retention & BSP subsector decoupling
│   ├── WALL_OCCLUSION_AND_ORIENTATION_ALIGNMENT.md # Winding order & coordinate fixes
│   └── FRAME_RETENTION_AND_BUFFER_STALL_RESOLUTION.md# Non-blocking UART & backdrop fills
│
└── sw/                                     # NEORV32 Runtime & Peripheral Drivers
    ├── common/                             # neorv32.ld, crt0.S, common.mk
    └── lib/                                # Peripheral driver headers & sources
```

---

## 7. Open-Source Verification & Workflow Reference

All operations can be driven via open-source tools without Vivado:

```bash
# 1. Compile firmware using the local RISC-V GCC toolchain
make build

# 2. Program the FPGA SRAM over JTAG via openFPGALoader
make flash-bit

# 3. Flash onboard SPI Flash memory for non-volatile boot
make flash-spi

# 4. Stream game binary over UART to the NEORV32 bootloader
make upload SERIAL_PORT=/dev/ttyUSB1 BAUD=19200
```
