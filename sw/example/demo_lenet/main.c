#include <neorv32.h>
#include <string.h>
#include "lenet_weights.h" // The exported PyTorch weights

#define BAUD_RATE 19200

// Memory buffers
int8_t IMAGE_BUF[3 * 32 * 32] __attribute__((aligned(32)));
int8_t X_UNROLLED[80 * 784] __attribute__((aligned(32))); // 80*784 for Conv1, fits all other layers
int32_t Y_BUF[16 * 784] __attribute__((aligned(32))); // Max size needed for padded intermediate matmul
int8_t FMAP_A[16 * 14 * 14] __attribute__((aligned(32))); // Max size for feature maps
int8_t FMAP_B[16 * 14 * 14] __attribute__((aligned(32))); 

static void print_dec32(uint32_t v) {
    if (v == 0) {
        neorv32_uart0_putc('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (v > 0) {
        buf[i++] = (v % 10) + '0';
        v /= 10;
    }
    while (i > 0) {
        neorv32_uart0_putc(buf[--i]);
    }
}

#include "../gemm_lib/gemmrv_hw.h"
extern void debug_dump();

// Reset hardware performance counters (write bit 3 of CMD register)
void hw_reset_perf_counters(void) {
    NEORV32_CFS->REG[6] = (1 << 3); // bit3 = counter reset
    NEORV32_CFS->REG[6] = 0;        // clear
}

// Read hardware performance counters into out[4]: fetch_a, fetch_b, compute, store
void hw_read_perf_counters(uint32_t out[4]) {
    out[0] = NEORV32_CFS->REG[0]; // perf_fetch_a_cycles
    out[1] = NEORV32_CFS->REG[1]; // perf_fetch_b_cycles
    out[2] = NEORV32_CFS->REG[2]; // perf_compute_cycles
    out[3] = NEORV32_CFS->REG[3]; // perf_store_cycles
}

void pad_and_copy_weights(int8_t* dst, const int8_t* src, int M, int K, int padded_M, int padded_K) {
    for(int m=0; m<padded_M; m++) {
        for(int k=0; k<padded_K; k++) {
            if (m < M && k < K) {
                dst[m*padded_K + k] = src[m*K + k];
            } else {
                dst[m*padded_K + k] = 0;
            }
        }
    }
}

// Image to Column transform for Convolution
void im2col(int8_t* image, int8_t* X, int in_c, int in_h, int in_w, int k, int padded_K, int padded_N) {
    int out_h = in_h - k + 1;
    int out_w = in_w - k + 1;
    int actual_N = out_h * out_w;
    int col = 0;
    for(int oh=0; oh<out_h; oh++) {
        for(int ow=0; ow<out_w; ow++) {
            int row = 0;
            for(int c=0; c<in_c; c++) {
                for(int kh=0; kh<k; kh++) {
                    for(int kw=0; kw<k; kw++) {
                        X[row * padded_N + col] = image[c*(in_h*in_w) + (oh+kh)*in_w + (ow+kw)];
                        row++;
                    }
                }
            }
            for(int p=row; p<padded_K; p++) {
                X[p * padded_N + col] = 0;
            }
            col++;
        }
    }
    for(int row=0; row<padded_K; row++) {
        for(int c=actual_N; c<padded_N; c++) {
            X[row * padded_N + c] = 0;
        }
    }
}

// CPU Fallback: Max Pooling 2x2
void maxpool2d(int8_t* in_fmap, int8_t* out_fmap, int c, int in_h, int in_w) {
    int out_h = in_h / 2;
    int out_w = in_w / 2;
    for(int ch=0; ch<c; ch++) {
        for(int oh=0; oh<out_h; oh++) {
            for(int ow=0; ow<out_w; ow++) {
                int8_t max_val = -128;
                for(int kh=0; kh<2; kh++) {
                    for(int kw=0; kw<2; kw++) {
                        int8_t val = in_fmap[ch*(in_h*in_w) + (oh*2+kh)*in_w + (ow*2+kw)];
                        if(val > max_val) max_val = val;
                    }
                }
                out_fmap[ch*(out_h*out_w) + oh*out_w + ow] = max_val;
            }
        }
    }
}

void apply_bias_relu_scale(int32_t* Y, int8_t* out_fmap, const int32_t* bias, int actual_M, int actual_N, int padded_N, int shift_bits) {
    for(int m=0; m<actual_M; m++) {
        int32_t b = bias[m];
        for(int n=0; n<actual_N; n++) {
            int32_t val = Y[m*padded_N + n] + b;
            if(val < 0) val = 0; // ReLU
            val = val >> shift_bits;
            if(val > 127) val = 127;
            out_fmap[m*actual_N + n] = (int8_t)val;
        }
    }
}

void gemm_sw(const int8_t* W, int8_t* X, int32_t* Y, int M, int K, int N) {
    for(int m=0; m<M; m++) {
        for(int n=0; n<N; n++) {
            int32_t sum = 0;
            for(int k=0; k<K; k++) {
                sum += (int32_t)W[m*K + k] * (int32_t)X[k*N + n];
            }
            Y[m*N + n] = sum;
        }
    }
}

void hw_multi_tile_test() {
    int8_t test_A[128]; // 8x16
    int8_t test_B[128]; // 16x8
    int32_t test_C_hw[64] = {0};
    int32_t test_C_sw[64] = {0};
    
    // Fill matrices
    for(int i=0; i<128; i++) {
        test_A[i] = -1;
        test_B[i] = 2;
    }
    // Set unique values to track accumulation across K=16
    test_A[0] = -5;  // k=0 tile
    test_A[8] = -10; // k=8 tile
    
    // SW execution
    gemm_sw(test_A, test_B, test_C_sw, 8, 16, 8);
    
    // HW execution (K=16 means two tiles)
    hw_gemm_tile(&test_A[0], &test_B[0], test_C_hw, 16, 8, 8, 8, 8, 8, 1, 0); // k=0, clear=1, store=0
    hw_gemm_tile(&test_A[8], &test_B[64], test_C_hw, 16, 8, 8, 8, 8, 8, 0, 1); // k=8, clear=0, store=1
    
    neorv32_uart0_printf("\n--- MULTI-TILE ACCUMULATION TEST ---\n");
    neorv32_uart0_printf("SW C[0..3]: %d, %d, %d, %d\n", test_C_sw[0], test_C_sw[1], test_C_sw[2], test_C_sw[3]);
    neorv32_uart0_printf("HW C[0..3]: %d, %d, %d, %d\n", test_C_hw[0], test_C_hw[1], test_C_hw[2], test_C_hw[3]);
    neorv32_uart0_printf("------------------------------------\n\n");
}

typedef struct {
    uint32_t im2col_cycles;
    uint32_t gemm_cycles;
    uint32_t other_cycles;
    uint32_t hw_fetch_a;
    uint32_t hw_fetch_b;
    uint32_t hw_compute;
    uint32_t hw_store;
} inference_stats_t;

int run_inference(int use_hw, inference_stats_t* stats, int32_t* out_scores) {
    uint32_t start_cycles, end_cycles;
    uint32_t im2col_cycles = 0;
    uint32_t gemm_cycles = 0;
    uint32_t other_cycles = 0;
    uint32_t hw_fetch_a = 0, hw_fetch_b = 0, hw_compute = 0, hw_store = 0;
    uint32_t perf[4];

    // Layer 1: Conv1 (3x32x32 -> 6x28x28)
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    im2col(IMAGE_BUF, X_UNROLLED, 3, 32, 32, 5, 80, 784); // padded K=80, N=784
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    im2col_cycles += (end_cycles - start_cycles);

    if(use_hw) {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        hw_reset_perf_counters();
        pad_and_copy_weights(FMAP_B, conv1_weight, 6, 75, 8, 80);
        gemmrv_mat A = GEMMRV_MAT(FMAP_B, 8, 80, 80);
        gemmrv_mat B = GEMMRV_MAT(X_UNROLLED, 80, 784, 784);
        gemmrv_mat_out C = GEMMRV_MAT_OUT(Y_BUF, 8, 784, 784);
        gemmrv_mult(&A, &B, &C);
        hw_read_perf_counters(perf);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
        hw_fetch_a += perf[0]; hw_fetch_b += perf[1]; hw_compute += perf[2]; hw_store += perf[3];
    } else {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_sw(conv1_weight, X_UNROLLED, Y_BUF, 6, 75, 784); // sw still uses unpadded if possible, but X is padded.
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
    }
    
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    // applying for SW too! SW needs padded_N=784
    apply_bias_relu_scale(Y_BUF, FMAP_A, conv1_bias, 6, 784, 784, SHIFT_CONV1);
    
    // Pool1 (6x28x28 -> 6x14x14)
    maxpool2d(FMAP_A, FMAP_B, 6, 28, 28);
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    other_cycles += (end_cycles - start_cycles);

    // Layer 2: Conv2 (6x14x14 -> 16x10x10)
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    im2col(FMAP_B, X_UNROLLED, 6, 14, 14, 5, 152, 104); // padded K=152, N=104
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    im2col_cycles += (end_cycles - start_cycles);
    
    if(use_hw) {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        hw_reset_perf_counters();
        pad_and_copy_weights(FMAP_B, conv2_weight, 16, 150, 16, 152);
        gemmrv_mat A2 = GEMMRV_MAT(FMAP_B, 16, 152, 152);
        gemmrv_mat B2 = GEMMRV_MAT(X_UNROLLED, 152, 104, 104);
        gemmrv_mat_out C2 = GEMMRV_MAT_OUT(Y_BUF, 16, 104, 104);
        gemmrv_mult(&A2, &B2, &C2);
        hw_read_perf_counters(perf);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
        hw_fetch_a += perf[0]; hw_fetch_b += perf[1]; hw_compute += perf[2]; hw_store += perf[3];
    } else {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_sw(conv2_weight, X_UNROLLED, Y_BUF, 16, 150, 104); 
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
    }
    
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    apply_bias_relu_scale(Y_BUF, FMAP_A, conv2_bias, 16, 100, 104, SHIFT_CONV2);
    
    // Pool2 (16x10x10 -> 16x5x5)
    maxpool2d(FMAP_A, FMAP_B, 16, 10, 10);
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    other_cycles += (end_cycles - start_cycles);

    // FC1 (400 -> 120)
    if(use_hw) {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        hw_reset_perf_counters();
        pad_and_copy_weights(X_UNROLLED, fc1_weight, 120, 400, 120, 400); 
        // Use custom FC offload where stride_B is 1 and N=8 to hardware
        gemmrv_mat A3 = GEMMRV_MAT(X_UNROLLED, 120, 400, 400);
        gemmrv_mat B3 = GEMMRV_MAT(FMAP_B, 400, 8, 1);
        gemmrv_mat_out C3 = GEMMRV_MAT_OUT(Y_BUF, 120, 8, 8);
        gemmrv_mult(&A3, &B3, &C3);
        hw_read_perf_counters(perf);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
        hw_fetch_a += perf[0]; hw_fetch_b += perf[1]; hw_compute += perf[2]; hw_store += perf[3];
    } else {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_sw(fc1_weight, FMAP_B, Y_BUF, 120, 400, 1);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
    }
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    int padded_N_fc = use_hw ? 8 : 1;
    apply_bias_relu_scale(Y_BUF, FMAP_A, fc1_bias, 120, 1, padded_N_fc, SHIFT_FC1);
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    other_cycles += (end_cycles - start_cycles);

    // FC2 (120 -> 84)
    if(use_hw) {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        hw_reset_perf_counters();
        pad_and_copy_weights(X_UNROLLED, fc2_weight, 84, 120, 88, 120); // M=88, K=120
        gemmrv_mat A4 = GEMMRV_MAT(X_UNROLLED, 88, 120, 120);
        gemmrv_mat B4 = GEMMRV_MAT(FMAP_A, 120, 8, 1);
        gemmrv_mat_out C4 = GEMMRV_MAT_OUT(Y_BUF, 88, 8, 8);
        gemmrv_mult(&A4, &B4, &C4);
        hw_read_perf_counters(perf);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
        hw_fetch_a += perf[0]; hw_fetch_b += perf[1]; hw_compute += perf[2]; hw_store += perf[3];
    } else {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_sw(fc2_weight, FMAP_A, Y_BUF, 84, 120, 1);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
    }
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    apply_bias_relu_scale(Y_BUF, FMAP_B, fc2_bias, 84, 1, padded_N_fc, SHIFT_FC2);
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    other_cycles += (end_cycles - start_cycles);

    // FC3 (84 -> 23)
    if(use_hw) {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        hw_reset_perf_counters();
        pad_and_copy_weights(X_UNROLLED, fc3_weight, 23, 84, 24, 88); // M=24, K=88
        gemmrv_mat A5 = GEMMRV_MAT(X_UNROLLED, 24, 88, 88);
        gemmrv_mat B5 = GEMMRV_MAT(FMAP_B, 88, 8, 1);
        gemmrv_mat_out C5 = GEMMRV_MAT_OUT(Y_BUF, 24, 8, 8);
        gemmrv_mult(&A5, &B5, &C5);
        hw_read_perf_counters(perf);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
        hw_fetch_a += perf[0]; hw_fetch_b += perf[1]; hw_compute += perf[2]; hw_store += perf[3];
    } else {
        start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_sw(fc3_weight, FMAP_B, Y_BUF, 23, 84, 1);
        end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_cycles += (end_cycles - start_cycles);
    }
    
    start_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    // Find Argmax (no ReLU/Scale on final output)
    int stride_fc3 = use_hw ? 8 : 1;
    int best_class = 0;
    int32_t best_score = Y_BUF[0 * stride_fc3] + fc3_bias[0];
    if(out_scores) out_scores[0] = best_score;
    for(int i=1; i<23; i++) {
        int32_t score = Y_BUF[i * stride_fc3] + fc3_bias[i];
        if(out_scores) out_scores[i] = score;
        if(score > best_score) {
            best_score = score;
            best_class = i;
        }
    }
    end_cycles = neorv32_cpu_csr_read(CSR_MCYCLE);
    other_cycles += (end_cycles - start_cycles);

    if(stats) {
        stats->im2col_cycles = im2col_cycles;
        stats->gemm_cycles   = gemm_cycles;
        stats->other_cycles  = other_cycles;
        stats->hw_fetch_a    = hw_fetch_a;
        stats->hw_fetch_b    = hw_fetch_b;
        stats->hw_compute    = hw_compute;
        stats->hw_store      = hw_store;
    }

    return best_class;
}

static const char* color_names[23] = {
    "dark-red",      // 0
    "white",         // 1
    "black",         // 2
    "orange",        // 3
    "silver-gray",   // 4
    "dark-blue",     // 5
    "grass-green",   // 6
    "red",           // 7
    "dark-gray",     // 8
    "gray",          // 9
    "brown",         // 10
    "cyan",          // 11
    "blue",          // 12
    "champagne",     // 13
    "dark-brown",    // 14
    "dark-orange",   // 15
    "pink",          // 16
    "lemon-yellow",  // 17
    "yellow",        // 18
    "earthy-yellow", // 19
    "red-orange",    // 20
    "green",         // 21
    "dark-green"     // 22
};

int main(void) {
    neorv32_rte_setup();
    neorv32_uart0_setup(BAUD_RATE, 0);
    neorv32_cpu_csr_write(CSR_MIE, 0);

    neorv32_uart0_printf("\n\n=== Edge-AI LeNet Inference ===\n");
    neorv32_uart0_printf("READY\n");

    inference_stats_t sw_stats, hw_stats;
    int32_t sw_scores[23];
    int32_t hw_scores[23];

    while (1) {
        neorv32_uart0_printf("AWAITING_IMAGE\n");
        for(int i = 0; i < 3072; i++) {
            IMAGE_BUF[i] = (int8_t)neorv32_uart0_getc();
        }
        neorv32_uart0_printf("IMAGE_RECEIVED\n");

        neorv32_uart0_printf("\n--- Running SW Inference ---\n");
        uint32_t sw_start = (uint32_t)neorv32_cpu_get_cycle();
        int sw_class = run_inference(0, &sw_stats, sw_scores);
        uint32_t sw_end = (uint32_t)neorv32_cpu_get_cycle();
        uint32_t sw_cycles = sw_end - sw_start;

        neorv32_uart0_printf("\n--- Running HW Inference ---\n");
        uint32_t hw_start = (uint32_t)neorv32_cpu_get_cycle();
        int hw_class = run_inference(1, &hw_stats, hw_scores);
        uint32_t hw_end = (uint32_t)neorv32_cpu_get_cycle();
        uint32_t hw_cycles = hw_end - hw_start;

        neorv32_uart0_printf("\n--- CPU Breakdown (SW) ---\n");
        neorv32_uart0_printf("im2col cycles: %u\n", sw_stats.im2col_cycles);
        neorv32_uart0_printf("gemm cycles:   %u\n", sw_stats.gemm_cycles);
        neorv32_uart0_printf("other cycles:  %u\n", sw_stats.other_cycles);
        neorv32_uart0_printf("---------------------------\n");

        neorv32_uart0_printf("\n--- CPU Breakdown (HW) ---\n");
        neorv32_uart0_printf("im2col cycles: %u\n", hw_stats.im2col_cycles);
        neorv32_uart0_printf("gemm cycles:   %u\n", hw_stats.gemm_cycles);
        neorv32_uart0_printf("other cycles:  %u\n", hw_stats.other_cycles);
        neorv32_uart0_printf("---------------------------\n");

        neorv32_uart0_printf("\n--- Results ---\n");
        neorv32_uart0_printf("SW Predicted Class ID: %d (%s)\n", sw_class, color_names[sw_class]);
        neorv32_uart0_printf("HW Predicted Class ID: %d (%s)\n", hw_class, color_names[hw_class]);

        neorv32_uart0_printf("\n--- Logit Scores (SW vs HW) ---\n");
        int max_diff = 0;
        for(int i = 0; i < 23; i++) {
            int diff = (int)hw_scores[i] - (int)sw_scores[i];
            if(diff < 0) diff = -diff;
            if(diff > max_diff) max_diff = diff;
            neorv32_uart0_printf("Class %d [%s]: SW=%d | HW=%d | diff=%d\n",
                                 i, color_names[i], (int)sw_scores[i], (int)hw_scores[i], (int)(hw_scores[i] - sw_scores[i]));
        }
        neorv32_uart0_printf("Max Absolute Error: %d\n", max_diff);
        
        neorv32_uart0_printf("\n--- Performance ---\n");
        neorv32_uart0_printf("Total SW Inference Cycles: %u\n", sw_cycles);
        neorv32_uart0_printf("Total HW Inference Cycles: %u\n", hw_cycles);
        
        uint32_t speedup_int = sw_cycles / hw_cycles;
        uint32_t speedup_dec = ((sw_cycles % hw_cycles) * 100) / hw_cycles;
        neorv32_uart0_printf("Real HW Speedup vs SW:     %u.%02ux\n", speedup_int, speedup_dec);

        
        neorv32_uart0_printf("\nBENCHMARK_COMPLETE\n");
        break;
    }

    return 0;
}
