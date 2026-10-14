# GEMMrv32

[![License](https://img.shields.io/github/license/stnolting/neorv32?label=License&style=flat-square)](https://github.com/stnolting/neorv32/blob/main/LICENSE)
[![Based On NEORV32](https://img.shields.io/badge/Based%20On-NEORV32-blue?style=flat-square)](https://github.com/stnolting/neorv32)
[![Platform](https://img.shields.io/badge/Platform-Nexys%20A7%20XC7A100T-orange?style=flat-square)](https://digilent.com/reference/programmable-logic/nexys-a7/start)
[![ISA](https://img.shields.io/badge/ISA-RV32IM-green?style=flat-square)](https://riscv.org)

GEMMrv32 is a hardware-accelerated bare-metal TinyML SoC built on the
[NEORV32](https://github.com/stnolting/neorv32) RISC-V processor. It extends
the base processor with a custom 8x8 systolic-array GEMM coprocessor, a
256-bit wide DMA memory crossbar, and a fully ported TensorFlow Lite Micro
runtime — all executing without an operating system.

The system is verified on a Nexys A7 FPGA (Artix-7 XC7A100T) and achieves a
**14.7x end-to-end neural network inference speedup** over software execution
on the same processor.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [What Was Added to NEORV32](#what-was-added-to-neorv32)
3. [System Architecture](#system-architecture)
4. [Hardware Modules](#hardware-modules)
5. [Software Components](#software-components)
6. [Performance Results](#performance-results)
7. [TensorFlow Lite Micro Port](#tensorflow-lite-micro-port)
8. [Hardware Utilization](#hardware-utilization)
9. [Repository Structure](#repository-structure)
10. [Building and Running](#building-and-running)
11. [Project Status](#project-status)
12. [License](#license)

---

## Project Overview

GEMMrv32 investigates hardware/software co-design for neural network inference
on embedded RISC-V processors. The project makes the following contributions
on top of the upstream NEORV32 processor:

- A custom 8x8 systolic-array GEMM accelerator integrated via the CFS port
- A 256-bit wide DMA memory crossbar for high-bandwidth tensor transfers
- A dedicated 256-bit wide BRAM for tensor storage
- A complete bare-metal TensorFlow Lite Micro execution environment
- A 23-class color classification neural network (LeNet variant) with a
  full hardware-accelerated inference pipeline and SW/HW correctness verification

All results are measured live on FPGA hardware using RISC-V cycle counters
(`mcycle` CSR) and communicated via UART.

---

## What Was Added to NEORV32

The following files are new additions to the upstream NEORV32 codebase.
Files that are modifications to existing upstream files are listed separately.

### New RTL Files

| File | Lines | Description |
|---|---:|---|
| `rtl/core/neorv32_cfs.vhd` | 385 | GEMM coprocessor: 8x8 systolic array, 2D strided DMA engine, bounds checker, control registers |
| `rtl/core/neorv32_memory_crossbar.vhd` | 160 | 2-master (CPU + DMA), 1-slave 256-bit bus arbiter |
| `rtl/core/neorv32_wide_bram.vhd` | 84 | 256-bit port BRAM for tensor storage |
| `rtl/core/neorv32_vga.vhd` | 211 | VGA raster controller: 160x120 dual-port VRAM, 4x hardware zoom to 640x480@60Hz, 256-color palette |
| `rtl/core/neorv32_ps2.vhd` | 145 | PS/2 keyboard receiver: digital deglitching filter, 11-bit frame decoder, 16-byte FIFO |

### New Software Files

| File | Lines | Description |
|---|---:|---|
| `sw/example/gemm_lib/gemmrv_hw.h` | 32 | Public C API for the hardware GEMM accelerator |
| `sw/example/gemm_lib/gemmrv_hw.c` | 78 | Driver: DMA setup, tile dispatch, polling, accumulation |
| `sw/example/gemm_lib/gemmrv_gpu.h` | 102 | Graphics, VGA, and PS/2 Keyboard C driver API |
| `sw/example/gemm_lib/gemmrv_gpu.c` | 170 | Driver implementation: VRAM primitives, Doom column drawer, 3D vertex transform, PS/2 parser |
| `sw/example/demo_gpu/main.c` | 98 | Standalone computer demo: VGA test patterns, color bars, interactive keyboard console |
| `sw/example/gemm_lib/README.md` | 27 | Library usage documentation |
| `sw/example/demo_gemm/main.c` | 81 | Standalone GEMM benchmark: randomized SW vs HW cycle comparison |
| `sw/example/demo_gemm/Makefile` | 33 | Build file for demo_gemm |
| `sw/example/demo_gemm/README.md` | 17 | Demo documentation |
| `sw/example/demo_lenet/main.c` | 443 | Full LeNet inference pipeline: im2col, SW GEMM, HW GEMM, comparison |
| `sw/example/demo_lenet/lenet_weights.h` | 4003 | Trained LeNet weight and bias arrays (int8 quantized) |
| `sw/example/demo_lenet/debug_dump.c` | 24 | Utility: hex dump activation tensors over UART |
| `sw/example/demo_lenet/Makefile` | 33 | Build file for demo_lenet |
| `sw/example/demo_lenet/README.md` | 31 | Inference pipeline documentation |
| `sw/example/demo_lenet/images/` | 24 files | 32x32 JPEG test images (24 color classes) |
| `sw/example/tflm_tree/build_tflm.py` | 78 | Multi-threaded Python build script for TFLite Micro cross-compilation |
| `sw/example/tflm_tree/tensorflow/` | ~456 files | Standalone TFLite Micro runtime tree (generated from upstream TFLM) |
| `sw/example/tflm_tree/third_party/` | — | Third-party dependencies: flatbuffers, gemmlowp, kissfft, ruy |
| `sw/basys/neorv32_basys3_top.vhd` | 142 | Basys 3 top wrapper: MMCM 50MHz clock generator, reset & pin wiring |
| `sw/basys/basys3.xdc` | 40 | Basys 3 FPGA constraints (Artix-7 XC7A35T) for I/O and QSPI flash |
| `sw/basys/build_basys3.tcl` | 59 | Vivado batch script to synthesize, implement, and export bitstream & SPI flash .bin |
| `sw/basys/flash_basys3.tcl` | 50 | Vivado Hardware Manager script to program onboard Macronix SPI flash |
| `sw/basys/upload_firmware.py` | 71 | Python utility for UART bootloader firmware upload |
| `sw/basys/README.md` | 45 | Basys 3 board bringup, build, and flashing guide |
| `docs/figures/gemm_architecture.png` | — | System architecture block diagram |

### Modified Upstream Files

The following upstream NEORV32 files were modified to wire in the custom hardware:

| File | Nature of Change |
|---|---|
| `rtl/core/neorv32_top.vhd` | Added 256-bit crossbar/BRAM instantiation in place of default DMEM; added CFS DMA bus signals; added CFS bus switch |
| `rtl/core/neorv32_package.vhd` | Added `sys2b_req`, `sys2b_rsp`, `cfs_req`, `cfs_rsp` signal declarations |
| `sw/common/neorv32.ld` | Set `__neorv32_rom_size = 256k` and `__neorv32_ram_size = 128k` for TFLM memory map |
| `sw/lib/source/neorv32_newlib.c` | Patched errno handling to resolve C++17/newlib link conflict |
| `rtl/core/neorv32_bootrom_image.vhd` | Updated bootloader image to match modified memory layout |
| `rtl/core/neorv32_imem_image.vhd` | Updated instruction memory image |

---

## System Architecture

```
+------------------------------------------------------------------+
|                         GEMMrv32 SoC                            |
|                                                                  |
|  +------------------+    32-bit bus    +---------------------+   |
|  |  NEORV32 CPU     |<--------------->|  Peripheral Bus     |   |
|  |  RV32IM @ 50MHz  |                 |  UART, GPIO, Timer  |   |
|  +--------+---------+                 +---------------------+   |
|           |                                                      |
|           | 32-bit (CPU master port)                             |
|  +--------v-----------------------------------------+           |
|  |        256-bit Memory Crossbar                   |           |
|  |        (neorv32_memory_crossbar.vhd)             |           |
|  |  Master 1: CPU (32-bit, word-aligned)            |<---+      |
|  |  Master 2: GEMM DMA (256-bit, burst)             |    |      |
|  +--------+-----------------------------------------+    |      |
|           |                                              |      |
|           | 256-bit slave port                           |      |
|  +--------v-----------------+                            |      |
|  |  Wide BRAM               |              +-------------+--+   |
|  |  (neorv32_wide_bram.vhd) |              |  GEMM CFS       |  |
|  |  Tensor storage          |              |  (neorv32_cfs)  |  |
|  |  128 KB capacity         |              |  8x8 systolic   |  |
|  +--------------------------+              |  64 parallel MACs|  |
|                                            |  2D strided DMA |   |
|  IMEM: 256 KB  (code + TFLM runtime)       +-----------------+  |
|  DMEM: 128 KB  (stack, heap, tensors)                           |
+------------------------------------------------------------------+
```

The CPU accesses tensors in BRAM via the crossbar at 32-bit width. The GEMM
DMA engine accesses the same BRAM via a 256-bit wide port, reading 32 bytes
per clock cycle. The crossbar arbitrates between both masters with the DMA
taking priority during active transfers.

---

## Hardware Modules

### neorv32_cfs.vhd — GEMM Coprocessor

The Custom Functions Subsystem implements the hardware GEMM accelerator. It
is memory-mapped to the CFS address space and controlled via CPU-written
configuration registers.

**Internal structure:**

- 8x8 systolic processing element array (64 parallel multiply-accumulate units)
- 2D strided DMA engine with configurable base address, row stride, and tile count
- Ping-pong input register bank for data reuse across the PE array
- Accumulator registers with bias addition and configurable output scaling
- Transfer-complete interrupt line to the CPU interrupt controller

**Control registers (memory-mapped, 32-bit):**

| Register | Offset | Function |
|---|---|---|
| `GEMM_CTRL` | 0x00 | Start bit, reset, busy status |
| `GEMM_A_BASE` | 0x04 | Base address of matrix A in BRAM |
| `GEMM_B_BASE` | 0x08 | Base address of matrix B in BRAM |
| `GEMM_C_BASE` | 0x0C | Destination address of output matrix C |
| `GEMM_DIM` | 0x10 | Matrix tile dimension (up to 8) |
| `GEMM_TILES` | 0x14 | Number of tiles for larger matrices |

### neorv32_memory_crossbar.vhd — 256-bit Bus Arbiter

Arbitrates access to the Wide BRAM between the CPU (Master 1, 32-bit) and the
GEMM DMA engine (Master 2, 256-bit). The crossbar performs width adaptation
between the two master ports. DMA requests are given priority to prevent
stalling the pipelined GEMM engine mid-transfer.

### neorv32_wide_bram.vhd — 256-bit Wide SRAM

Implements a synchronous BRAM with a 256-bit data port (32 bytes per access).
Used exclusively for tensor storage. Both the CPU and GEMM DMA access this
memory through the crossbar. The wide port allows the DMA to load a full 8-element
int8 row in a single clock cycle, sustaining the systolic array pipeline.

---

## Software Components

### gemm_lib — Hardware Accelerator Driver

The C driver library exposes a simple function interface over the memory-mapped
GEMM registers.

```c
// Multiply an M x K matrix A by a K x N matrix B, store result in C
// Uses hardware tiling for matrices larger than 8x8
void gemmrv_hw_matmul(const int8_t *A, const int8_t *B, int32_t *C,
                      int M, int K, int N);

// Poll until the GEMM engine reports completion
void gemmrv_hw_wait(void);
```

The driver handles: DMA address configuration, tile dispatch, inter-tile
accumulation, and result readback. All tensor buffers must reside in the
internal BRAM region (base address `0x80000000`) for the 256-bit DMA to
function. DDR2 external memory is not accessible via the DMA path.

### demo_lenet — LeNet Inference Benchmark

Implements a complete end-to-end neural network inference benchmark on the
Artix-7 FPGA. The model is a LeNet-variant trained to classify 32x32 RGB
images into 23 colour categories.

The benchmark runs the same input image through two identical inference paths:

1. **Software path** — all matrix multiplication implemented in C with the
   RISC-V CPU executing scalar multiply-accumulate loops.
2. **Hardware path** — im2col output is passed to the GEMM driver, which
   dispatches tiles to the hardware accelerator.

Both paths produce logit vectors that are compared element-by-element. The
maximum absolute error between all output logits is printed alongside the
cycle counts for each stage.

The test image (32x32 RGB, 3072 bytes) is sent over UART after boot and
processed without any buffering to external memory.

### tflm_tree — TensorFlow Lite Micro Bare-Metal Port

A complete port of the TensorFlow Lite Micro runtime to the GEMMrv32 bare-metal
environment. The runtime is compiled as a static C++17 library and linked with
the NEORV32 HAL and newlib.

**Key porting decisions:**

| Issue | Solution |
|---|---|
| Standard NEORV32 Makefile flattens object files by name, causing collisions between TFLM files with identical names (e.g. `common.cpp` appears in multiple subdirectories) | Custom Python build script (`build_tflm.py`) that prefixes each object file with its full directory path |
| TFLM requires C++17 (`if constexpr`, `std::is_integral_v`) | Compile with `-std=c++17` throughout |
| `vfprintf(stderr, ...)` in `debug_log.cpp` requires a functional newlib `stderr` | `system_setup.cpp` patched to call `neorv32_uart0_setup()` before the TFLM allocator initialises |
| TFLM + HAL binary size exceeds 128 KB default IMEM | `IMEM_SIZE` increased to 256 KB in VHDL; linker script updated accordingly |
| newlib errno conflict with C++ headers | `#undef errno` / `extern int errno` removed from `neorv32_newlib.c` |

The standalone TFLM tree is generated using the upstream
`tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py` script
with the `--rename_cc_to_cpp` flag.

---

## Performance Results

All measurements are taken on the Nexys A7 FPGA at 50 MHz using the
RISC-V `mcycle` performance counter. No caches are used. Results are
transmitted over UART at 19200 baud.

### Raw GEMM Kernel Speedup

CPU baseline: `rv32im_zicsr_zifencei`, `-Os` optimisation.

| Matrix Dimension | MAC Operations | CPU Cycles | Accelerator Cycles | Speedup |
|:---:|---:|---:|---:|---:|
| 8x8 | 512 | 17,030 | 517 | **32x** |
| 16x16 | 4,096 | 128,822 | 2,543 | **50x** |
| 24x24 | 13,824 | 427,646 | 7,261 | **58x** |

Speedup increases with matrix dimension because the DMA setup overhead is
amortized over more tiles, and data reuse in the systolic array becomes
more effective.

### Optimized CPU Baseline (16x16)

CPU configured as `rv32imc_zba_zbb_zbs_zicsr_zifencei`, compiled with
`-O3` and `#pragma GCC unroll 16` on the inner loop.

| Architecture | Execution Cycles | Speedup vs. Accelerator |
|---|---:|---:|
| CPU (`-Os`) | 128,822 | baseline |
| CPU (`-O3` + loop unrolling) | 66,198 | 1.94x faster than `-Os` |
| GEMM Accelerator | 2,527 | **26.2x faster than optimized CPU** |

Compiler optimisation halved the CPU cycle count. The hardware accelerator
maintains a greater than 26x advantage over the best software implementation.

### End-to-End LeNet Inference (23-Class Color Classifier)

The benchmark runs a complete LeNet inference on a 32x32 RGB input image.
The same image is processed by the software path and the hardware path in
the same boot session. Cycle counts cover im2col, GEMM, activation, and
pooling stages.

| Stage | Software Cycles | Hardware Cycles | Notes |
|---|---:|---:|---|
| im2col | 914,018 | 914,018 | CPU-executed, identical in both paths |
| **GEMM** | **38,552,183** | **941,466** | **40.9x kernel speedup** |
| Other (activation, pool) | 886,718 | 886,370 | Near-identical |
| **Total inference** | **40,353,347** | **2,742,298** | — |
| **End-to-end speedup** | — | **14.7x** | — |

**Correctness verification:**

| Metric | Result |
|---|---|
| SW predicted class | Correct |
| HW predicted class | Correct (identical to SW) |
| Maximum absolute logit error (all 23 classes) | **0** |

The maximum absolute error across all output logits between the software and
hardware paths is zero, confirming bit-exact numerical equivalence.

#### UART Output (captured from Nexys A7, 5 October 2026)

```
=== Edge-AI LeNet Inference ===
READY
IMAGE_RECEIVED

--- CPU Breakdown (SW) ---
im2col cycles: 914018
gemm cycles:   38552183
other cycles:  886718
---------------------------

--- CPU Breakdown (HW) ---
im2col cycles: 914018
gemm cycles:   941466
other cycles:  886370
---------------------------

--- Results ---
SW Predicted Class ID: 17 (lemon-yellow)
HW Predicted Class ID: 17 (lemon-yellow)

Max Absolute Error: 0

--- Performance ---
Total SW Inference Cycles: 40353347
Total HW Inference Cycles: 2742298
Real HW Speedup vs SW:     14.7x

BENCHMARK_COMPLETE
```

### Accelerator Utilisation

The 256-bit DMA delivers a 32-byte row to the systolic array each clock cycle.
The 8x8 array can sustain 64 MACs per cycle at peak. Measured effective
throughput over a full LeNet inference:

| Metric | Value |
|---|---|
| Theoretical PE throughput | 64 MACs/cycle |
| Effective throughput (measured) | 1.62 MACs/cycle |
| Utilisation | ~2.5% of theoretical peak |

The low utilisation is caused by the 32-bit CPU bus width limiting tensor
pre-loading speed and by configuration overhead per tile. The 256-bit DMA
path addresses this for the BRAM-resident operand, but loading the weight
matrix still bottlenecks through the CPU. This is the primary target for
future improvement.

### Linux Kernel Boot Log (noMMU RISC-V 32)

GEMMrv32 has also been validated with noMMU Linux (Kernel 6.6.83) running on the NEORV32 core with 32 MB external SDRAM. The kernel image, device tree (DTB), and initramfs are staged and booted directly via the stage2 xmodem loader.

#### UART Boot Log Capture (115200 baud)

```
[stage2] ready - RV32IMAC NEORV32 loader
[stage2] CLK=0x02faf080
[stage2] SDRAM word-write test...
[stage2] SDRAM test PASS
[stage2] Mode: Linux direct boot
[stage2] Memory layout:
  Kernel:    0x40000000
  DTB:       0x41f00000
  Initramfs: 0x41f80000
[stage2] All payloads loaded:
  Kernel:  0x00162600 bytes
  DTB:     0x00000600 bytes
  Initrd:  0x00000780 bytes

[stage2] Booting Linux...
  a0 = 0 (hartid)
  a1 = 0x41f00000 (DTB)
  PC = 0x40000000 (kernel)
[stage2] SDRAM exec test @0x40162600
SDRAM exec OK

[    0.000000] Linux version 6.6.83-g7d6073865396-dirty (test@H3C) (riscv-none-elf-gcc (xPack GNU RISC-V Embedded GCC x86_64) 14.2.0, GNU ld (xPack GNU RISC-V Embedded GCC x86_64) 2.43.1)
[    0.000000] Machine model: NEORV32 on HeiJin AX301 (EP4CE6)
[    0.000000] Forcing kernel command line to: earlycon=neorv32,0xfff50000 console=ttyNEO0,115200
[    0.000000] earlycon: neorv32 at MMIO 0xfff50000 (options '')
[    0.000000] printk: bootconsole [neorv32] enabled
[    0.000000] Zone ranges:
[    0.000000]   Normal   [mem 0x0000000040000000-0x0000000041ffffff]
[    0.000000] Movable zone start for each node
[    0.000000] Early memory node ranges
[    0.000000]   node   0: [mem 0x0000000040000000-0x0000000041ffffff]
[    0.000000] Initmem setup node 0 [mem 0x0000000040000000-0x0000000041ffffff]
[    0.000000] riscv: base ISA extensions acim
[    0.000000] riscv: ELF capabilities acim
[    0.000000] Kernel command line: earlycon=neorv32,0xfff50000 console=ttyNEO0,115200
[    0.000000] Dentry cache hash table entries: 4096 (order: 2, 16384 bytes, linear)
[    0.000000] Inode-cache hash table entries: 2048 (order: 1, 8192 bytes, linear)
[    0.000000] Built 1 zonelists, mobility grouping on.  Total pages: 8128
[    0.000000] mem auto-init: stack:all(zero), heap alloc:off, heap free:off
[    0.000000] Memory: 30968K/32768K available (1035K kernel code, 125K rwdata, 155K rodata, 96K init, 49K bss, 1800K reserved, 0K cma-reserved)
[    0.000000] SLUB: HWalign=64, Order=0-1, MinObjects=0, CPUs=1, Nodes=1
[    0.000000] NR_IRQS: 64, nr_irqs: 64, preallocated irqs: 0
[    0.000000] riscv-intc: 32 local interrupts mapped
[    0.000000] clint: clint@fff40000: timer running at 50000000 Hz
[    0.000000] clocksource: clint_clocksource: mask: 0xffffffffffffffff max_cycles: 0xb8812736b, max_idle_ns: 440795202655 ns
[    0.000536] sched_clock: 64 bits at 50MHz, resolution 20ns, wraps every 4398046511100ns
[    0.062703] Calibrating delay loop (skipped), value calculated using timer frequency.. 100.00 BogoMIPS (lpj=200000)
[    0.107261] pid_max: default: 4096 minimum: 301
[    0.180255] Mount-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    0.221219] Mountpoint-cache hash table entries: 1024 (order: 0, 4096 bytes, linear)
[    2.608665] clocksource: jiffies: mask: 0xffffffff max_cycles: 0xffffffff, max_idle_ns: 7645041785100000 ns
[    2.652888] futex hash table entries: 16 (order: -5, 192 bytes, linear)
[    7.896570] cpu0: Ratio of byte access time to unaligned word access is 0.00, unaligned accesses are slow
[    9.471223] clocksource: Switched to clocksource clint_clocksource
...
========================================
 NEORV32 nommu Linux — mini shell
========================================
Linux (none) 6.6.83-... riscv32
Uptime:    18 s
Total RAM: 31000 KB
Free RAM:  30240 KB
Processes: 15

Type 'help' for commands.

nommu#
```

---

## TensorFlow Lite Micro Port

The TFLite Micro (TFLM) runtime has been ported to run bare-metal on the
NEORV32 core. The binary is uploaded over UART via the NEORV32 bootloader
and executes directly from the 256 KB instruction memory.

### Build Environment

| Item | Value |
|---|---|
| Compiler | `riscv64-unknown-elf-gcc` (Vitis 2024.2 toolchain) |
| C++ standard | C++17 |
| ISA | `rv32im` |
| ABI | `ilp32` |
| Optimisation | `-Os` |
| Binary size | ~135 KB |
| IMEM required | 256 KB |
| DMEM required | 128 KB (tensor arena: 1,376 bytes for `hello_world`) |

### TFLM hello_world Result (captured from Nexys A7, 5 October 2026)

```
Booting (@0x00000000)...

"Unique Tag","Total ticks across all events with that tag."
FULLY_CONNECTED, 0
"total number of ticks", 0

[RecordingMicroAllocator] Arena allocation total 1376 bytes
[RecordingMicroAllocator] Arena allocation head 128 bytes
[RecordingMicroAllocator] Arena allocation tail 1248 bytes
[RecordingMicroAllocator] 'TfLiteEvalTensor data' used 120 bytes
[RecordingMicroAllocator] 'Persistent TfLiteTensor data' used 80 bytes
[RecordingMicroAllocator] 'Persistent buffer data' used 668 bytes
[RecordingMicroAllocator] 'NodeAndRegistration struct' used 96 bytes

~~~ALL TESTS PASSED~~~
```

The TFLM allocator, interpreter, and op resolver are all live and functional.
The `RecordingMicroAllocator` output confirms the full tensor arena, node
registration, and evaluation tensor allocation chains are operating correctly.

---

## Hardware Utilization

### Basys 3 (Artix-7 XC7A35T) — GEMMrv32-DOOM Standalone Workstation
Synthesized and implemented at **50 MHz** using Vivado 2025.2 in batch mode (`sw/basys/build_basys3.tcl`).

| Resource | Used | Available | Utilization | Notes |
|---|---:|---:|---:|---|
| **Block RAM Tile (`RAMB36`)** | **42.5** | **50** | **85.0%** | Houses 64 KB IMEM, 64 KB DMEM (256-bit wide), and 19.2 KB VRAM |
| **Slice LUT** | **8,096** | **20,800** | **38.9%** | Includes 8x8 systolic array, XBUS arbiter, VGA raster & PS/2 |
| **Slice FF (Registers)** | **5,380** | **41,600** | **12.9%** | Core registers & pipeline stages |
| **LUT as Distributed RAM** | **30** | **9,600** | **0.3%** | Clean BRAM mapping without slice memory spill |
| **DSP48E1** | **0** | **90** | **0.0%** | Arithmetic blocks implemented in fabric |
| **Worst Negative Slack (WNS)** | **+4.207 ns** | — | — | **Timing cleanly closed @ 50 MHz** (Zero timing violations) |

### Generated Bitstream & Configuration Artifacts (`sw/basys/build/`)

| Artifact Name | Size | Function / Description |
|---|---:|---|
| `neorv32_basys3.bit` | 1,128,624 B | Raw FPGA SRAM configuration bitstream. Programmed via Vivado Hardware Manager / JTAG for instantaneous volatile bring-up. |
| `neorv32_basys3.bin` | 1,128,480 B | Quad-SPI (`SPIx4`) configuration binary. Programmed into onboard Macronix `MX25L3233F` flash (4 MB) for automatic boot on power-up. |
| `timing_summary.rpt` | 39,799 B | Full multi-corner timing report confirming zero setup/hold violations (`WNS = +4.207 ns`, `WHS = +0.037 ns`). |
| `utilization.rpt` | 11,706 B | Post-implementation resource report. |

---

### Nexys A7 (Artix-7 XC7A100T) Reference
Synthesis and implementation targeting Nexys A7 (Artix-7 XC7A100T) at 50 MHz:

| Resource | Used | Available | Utilization |
|---|---:|---:|---:|
| Slice LUT | ~7,200 | 63,400 | ~11% |
| Slice FF | ~4,100 | 126,800 | ~3% |
| BRAM (36K) | ~12 | 135 | ~9% |
| DSP48E1 | ~16 | 240 | ~7% |

---

## Repository Structure

```
GEMMrv32/
|
+-- rtl/
|   +-- core/
|       +-- neorv32_cfs.vhd                (ADDED) GEMM coprocessor
|       +-- neorv32_memory_crossbar.vhd    (ADDED) 256-bit DMA crossbar
|       +-- neorv32_wide_bram.vhd          (ADDED) 256-bit wide BRAM
|       +-- neorv32_top.vhd                (MODIFIED) CFS + crossbar wiring
|       +-- neorv32_package.vhd            (MODIFIED) Signal declarations
|       +-- neorv32_bootrom_image.vhd      (MODIFIED) Updated image
|       +-- neorv32_imem_image.vhd         (MODIFIED) Updated image
|       +-- neorv32_bus.vhd                (upstream NEORV32)
|       +-- neorv32_cpu*.vhd               (upstream NEORV32)
|       +-- ...
|
+-- sw/
|   +-- example/
|       +-- gemm_lib/                      (ADDED) Hardware accelerator driver
|       |   +-- gemmrv_hw.h
|       |   +-- gemmrv_hw.c
|       |   +-- README.md
|       |
|       +-- demo_gemm/                     (ADDED) Standalone GEMM benchmark
|       |   +-- main.c
|       |   +-- Makefile
|       |   +-- README.md
|       |
|       +-- demo_lenet/                    (ADDED) Full inference benchmark
|       |   +-- main.c
|       |   +-- lenet_weights.h
|       |   +-- debug_dump.c
|       |   +-- images/  (24 JPEG test images)
|       |   +-- Makefile
|       |   +-- README.md
|       |
|       +-- tflm_tree/                     (ADDED) TFLite Micro bare-metal port
|           +-- build_tflm.py              Custom build script
|           +-- tensorflow/                TFLM standalone runtime (~456 files)
|           +-- third_party/               flatbuffers, gemmlowp, kissfft, ruy
|
|   +-- common/
|   |   +-- neorv32.ld                     (MODIFIED) 256K ROM / 128K RAM layout
|   |
|   +-- lib/source/
|       +-- neorv32_newlib.c               (MODIFIED) C++17 errno fix
|
+-- docs/
    +-- figures/
        +-- gemm_architecture.png          (ADDED) Architecture block diagram
        +-- ...                            (upstream NEORV32 documentation)
```

---

## Building and Running

### Prerequisites

| Tool | Purpose |
|---|---|
| Vivado 2024.2 | FPGA synthesis and programming |
| `riscv64-unknown-elf-gcc` | Bare-metal cross-compiler (Vitis 2024.2 bundled) |
| Python 3 + `pyserial` | UART upload scripts |
| Nexys A7 board | Target hardware (Artix-7 XC7A100T) |

### Flash FPGA Bitstream

```bash
vivado -mode batch -source program_fpga.tcl
```

### Build and Run GEMM Benchmark

```bash
cd sw/example/demo_gemm
make
# Binary is generated at neorv32_exe.bin
# Upload via bootloader: send 'u', stream binary, send 'e'
```

### Build and Run LeNet Inference

```bash
cd sw/example/demo_lenet
make
# Upload neorv32_exe.bin via bootloader
# After 'e', send 3072 bytes of preprocessed image data over UART
```

### Build TensorFlow Lite Micro Runtime

```bash
cd sw/example/tflm_tree
python3 build_tflm.py
# Output: build/neorv32_exe_final.bin
# Upload via bootloader
```

### Bootloader Protocol

The NEORV32 bootloader communicates at 19200 baud on UART0. To upload and run
a binary:

1. Send `u` to enter upload mode
2. Wait for `Awaiting neorv32_exe.bin...`
3. Stream the binary file
4. Wait for `OK`
5. Send `e` to execute

---

## Project Status

| Feature | Status |
|---|---|
| 8x8 systolic-array GEMM accelerator (CFS) | Complete |
| 256-bit DMA memory crossbar | Complete |
| 256-bit wide BRAM for tensor storage | Complete |
| C software driver (gemmrv_hw.c / .h) | Complete |
| 32x / 50x / 58x raw GEMM kernel speedup | Verified on hardware |
| 26.2x speedup over -O3 optimised CPU | Verified on hardware |
| 14.7x end-to-end LeNet inference speedup | Verified on hardware |
| Bit-exact SW vs HW verification (max error = 0) | Verified on hardware |
| TensorFlow Lite Micro bare-metal port | Complete |
| TFLM hello_world executing on RV32IM without OS | Verified on hardware |
| TFLM FullyConnected kernel patched to use GEMM hardware | In progress |
| Larger TFLite models (MobileNet, DS-CNN) | Planned |

---

## NEORV32 Foundation

GEMMrv32 is built on the NEORV32 processor by Stephan Nolting. NEORV32
provides a fully platform-independent VHDL SoC with a configurable RV32
processor, memory subsystem, peripheral bus, on-chip debugger, and
complete software toolchain support.

For complete NEORV32 documentation:

- [NEORV32 Datasheet](https://stnolting.github.io/neorv32/)
- [NEORV32 User Guide](https://stnolting.github.io/neorv32/ug/)
- [NEORV32 GitHub Repository](https://github.com/stnolting/neorv32)

---

## Contributing

Contributions are welcome. To contribute, open an issue or submit a pull
request. See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

<a href="https://github.com/eatheswar/GEMMrv32/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=eatheswar/GEMMrv32" />
</a>

GEMMrv32 gratefully acknowledges the NEORV32 upstream project and community
for the processor architecture, tooling, and documentation that make this
work possible.

---

## License

GEMMrv32 is based on the open-source NEORV32 project.
See the [NEORV32 license](https://github.com/stnolting/neorv32/blob/main/LICENSE)
for licensing details.
