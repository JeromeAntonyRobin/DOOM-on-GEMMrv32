# Massive Matrix Multiplication Demo

This directory contains a standalone benchmarking program to demonstrate the raw throughput of the GEMM hardware accelerator against the RISC-V CPU.

It initializes two massive `64x64` matrices (the largest size that fits comfortably within the FPGA's internal Block RAM) and computes the multiplication using both hardware and software to verify math parity.

## Quickstart

1. **Compile the executable:**
```bash
make clean_all exe
```

2. **Upload and Run:**
You can stream the `neorv32_exe.bin` to your board over UART using the bootloader, or run the `auto_run_demo.py` script provided in the repository root.

**Expected Speedup:** ~70x hardware acceleration over the host CPU.
