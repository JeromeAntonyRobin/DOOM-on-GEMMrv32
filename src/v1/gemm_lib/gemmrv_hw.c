#include "gemmrv_hw.h"
#include <neorv32.h>

// Low-level hardware tile function (internal)
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
    
    while ((NEORV32_CFS->REG[6] & (1 << 2)) == 0) {} // Wait for status_done=1
    
    NEORV32_CFS->REG[6] = 0; // clear cmd_start
    while ((NEORV32_CFS->REG[6] & (1 << 2)) != 0) {} // Wait for status_done=0 (IDLE)
    
    asm volatile ("fence" ::: "memory");
}

void gemmrv_mult(gemmrv_mat* A, gemmrv_mat* B, gemmrv_mat_out* C) {
    int M = A->rows;
    int K = A->cols;  // K is cols of A
    int N = B->cols;  // N is cols of B
    
    for(int m = 0; m < M; m += 8) {
        int tile_M = (M - m < 8) ? (M - m) : 8;
        for(int n = 0; n < N; n += 8) {
            int tile_N = (N - n < 8) ? (N - n) : 8;
            for(int k = 0; k < K; k += 8) {
                int tile_K = (K - k < 8) ? (K - k) : 8;
                
                const int8_t* A_ptr = &(A->data[m * A->stride + k]);
                const int8_t* B_ptr = &(B->data[k * B->stride + n]);
                int32_t* C_ptr = &(C->data[m * C->stride + n]);
                
                hw_gemm_tile(A_ptr, B_ptr, C_ptr, 
                             A->stride, B->stride, C->stride, 
                             tile_M, tile_K, tile_N, 
                             (k == 0),          // Clear accumulator on first K tile
                             (k + 8 >= K));     // Store result on last K tile
            }
        }
    }
}

void gemmrv_post_process(gemmrv_mat_out* C, const int32_t* bias, int shift, int enable_relu) {
    for (int m = 0; m < C->rows; m++) {
        for (int n = 0; n < C->cols; n++) {
            int32_t val = C->data[m * C->stride + n];
            
            if (bias) {
                val += bias[n];
            }
            if (enable_relu && val < 0) {
                val = 0;
            }
            
            // Arithmetic right shift for quantization scaling
            if (shift > 0) {
                val = val >> shift;
            }
            
            C->data[m * C->stride + n] = val;
        }
    }
}
