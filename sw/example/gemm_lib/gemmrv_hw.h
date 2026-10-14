#ifndef GEMMRV_HW_H
#define GEMMRV_HW_H

#include <stdint.h>

// The Custom Hardware struct for matrix operations
typedef struct {
    const int8_t* data;  // Pointer to flat memory array
    int rows;            // True number of rows
    int cols;            // True number of columns
    int stride;          // Padded width (must be multiple of 8 for hardware)
} gemmrv_mat;

// Output matrix (32-bit accumulation)
typedef struct {
    int32_t* data;
    int rows;
    int cols;
    int stride;
} gemmrv_mat_out;

// Strictly Hardware Accelerated Matrix Multiplication: C = A * B
// Note: Matrices MUST have strides that are multiples of 8.
void gemmrv_mult(gemmrv_mat* A, gemmrv_mat* B, gemmrv_mat_out* C);

// CPU Software Helper: Apply Bias, ReLU, and shift (quantization scaling)
void gemmrv_post_process(gemmrv_mat_out* C, const int32_t* bias, int shift, int enable_relu);

#endif // GEMMRV_HW_H

#define GEMMRV_MAT(ptr, r, c, s) ((gemmrv_mat){ .data = (ptr), .rows = (r), .cols = (c), .stride = (s) })
#define GEMMRV_MAT_OUT(ptr, r, c, s) ((gemmrv_mat_out){ .data = (ptr), .rows = (r), .cols = (c), .stride = (s) })
