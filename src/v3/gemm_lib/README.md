# GEMM Hardware Accelerator Library

This folder contains a pure C library (`gemmrv_hw.c` / `gemmrv_hw.h`) for interfacing with the custom systolic-array Matrix Multiplication hardware.

## Features
- **Zero CPU overhead**: Matrices are packed in `gemmrv_mat` structs directly aligned to hardware constraints.
- **Support for vectors**: Vectors can seamlessly be cast into matrices by setting the `stride` parameter to 1.
- **Software Post-processing**: Includes `gemmrv_post_process` for Bias, ReLU, and shift operations common in Quantized Neural Networks.

## How to use
Just wrap your padded flat memory arrays inside a `GEMMRV_MAT` macro and run the hardware!

```c
#include "gemmrv_hw.h"

// 1. Setup padded arrays
int8_t raw_A[8 * 8] = { 1, 2, 3, 0, 0, 0, 0, 0 }; // 1x3 matrix (padded to 8 wide)
int8_t raw_B[8 * 8] = { 4, 5, 6, 0, 0, 0, 0, 0 }; // 3x1 matrix

// 2. Package into Structs
gemmrv_mat A = GEMMRV_MAT(raw_A, 1, 3, 8);
gemmrv_mat B = GEMMRV_MAT(raw_B, 3, 1, 8);
gemmrv_mat_out C = GEMMRV_MAT_OUT(raw_C, 1, 1, 8);

// 3. Offload!
gemmrv_mult(&A, &B, &C);
```
