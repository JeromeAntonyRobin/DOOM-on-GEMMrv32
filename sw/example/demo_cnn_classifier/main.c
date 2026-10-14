#include <stdint.h>
#include <neorv32.h>
#include "../gemm_lib/gemmrv_hw.h"
#include "cnn_model_weights.h"

// Input: 28x28 image (INT8 values normalized from -128 to 127)
static int8_t img_input[28 * 28] __attribute__((aligned(32)));

// Layer 1 im2col buffer: M=144, K=32 (padded from 25 for 8x8 GEMM alignment)
static int8_t col1_buf[144 * 32] __attribute__((aligned(32)));
// Layer 1 output: 144 x 8 (32-bit accumulated)
static int32_t out1_buf_hw[144 * 8] __attribute__((aligned(32)));
static int32_t out1_buf_sw[144 * 8] __attribute__((aligned(32)));
// Layer 1 quantized activation (ReLU + >> 6): 12 x 12 x 8 (1152 bytes)
static int8_t feat1_act[12 * 12 * 8] __attribute__((aligned(32)));

// Layer 2 im2col buffer: M=32 (padded from 25 to 32 for 8x8 GEMM), K=72 (9*8)
static int8_t col2_buf[32 * 72] __attribute__((aligned(32)));
// Layer 2 output: 32 x 16 (32-bit accumulated)
static int32_t out2_buf_hw[32 * 16] __attribute__((aligned(32)));
static int32_t out2_buf_sw[32 * 16] __attribute__((aligned(32)));
// Layer 2 flattened activation (5 x 5 x 16 = 400 features): padded to 400
static int8_t feat2_act[400] __attribute__((aligned(32)));

// Layer 3 (FC): Input M=8 (padded from 1 to 8), K=400 (50*8), N=40 (5*8)
static int8_t fc_in[8 * 400] __attribute__((aligned(32)));
// FC Output: 8 x 40
static int32_t fc_out_hw[8 * 40] __attribute__((aligned(32)));
static int32_t fc_out_sw[8 * 40] __attribute__((aligned(32)));

// Software GEMM baseline for speedup comparison
void gemm_sw(const int8_t* A, const int8_t* B, int32_t* C, int M, int K, int N) {
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            int32_t acc = 0;
            for (int k = 0; k < K; k++) {
                acc += (int32_t)A[m * K + k] * (int32_t)B[k * N + n];
            }
            C[m * N + n] = acc;
        }
    }
}

// Prepare Layer 1 im2col
void prepare_im2col1(const int8_t* img) {
    int row_idx = 0;
    for (int r = 0; r < 24; r += 2) {
        for (int c = 0; c < 24; c += 2) {
            int8_t* dst = &col1_buf[row_idx * 32];
            int p = 0;
            for (int dr = 0; dr < 5; dr++) {
                for (int dc = 0; dc < 5; dc++) {
                    dst[p++] = img[(r + dr) * 28 + (c + dc)];
                }
            }
            while (p < 32) dst[p++] = 0; // Pad to 32
            row_idx++;
        }
    }
}

// Prepare Layer 2 im2col
void prepare_im2col2(const int8_t* feat) {
    // feat is 12 x 12 x 8
    int row_idx = 0;
    for (int r = 0; r < 9; r += 2) {
        for (int c = 0; c < 9; c += 2) {
            int8_t* dst = &col2_buf[row_idx * 72];
            int p = 0;
            for (int dr = 0; dr < 3; dr++) {
                for (int dc = 0; dc < 3; dc++) {
                    for (int ch = 0; ch < 8; ch++) {
                        dst[p++] = feat[((r + dr) * 12 + (c + dc)) * 8 + ch];
                    }
                }
            }
            row_idx++;
        }
    }
    // Pad remaining rows up to 32
    for (; row_idx < 32; row_idx++) {
        for (int k = 0; k < 72; k++) col2_buf[row_idx * 72 + k] = 0;
    }
}

int main() {
    neorv32_gpio_port_set(0x01);
    neorv32_uart0_setup(19200, 0);
    neorv32_gpio_port_set(0x03);

    neorv32_uart0_printf("\n\n=======================================================\n");
    neorv32_uart0_printf("=== NEORV32 8x8 SYSTOLIC CNN ACCELERATOR DASHBOARD ===\n");
    neorv32_uart0_printf("=== 36-Class Classifier (Digits 0-9 & Letters A-Z)   ===\n");
    neorv32_uart0_printf("=== Architecture: Conv1 (5x5) -> Conv2 -> Dense FC   ===\n");
    neorv32_uart0_printf("=======================================================\n");

    while (1) {
        neorv32_uart0_printf("\nREADY_FOR_IMAGE\n");

        // Wait for protocol sync: '!IMG'
        while (1) {
            if (neorv32_uart0_getc() == '!' &&
                neorv32_uart0_getc() == 'I' &&
                neorv32_uart0_getc() == 'M' &&
                neorv32_uart0_getc() == 'G') {
                break;
            }
        }

        // Read 784 bytes (28x28)
        for (int i = 0; i < 28 * 28; i++) {
            img_input[i] = (int8_t)neorv32_uart0_getc();
        }

        neorv32_gpio_port_set(0x07);

        // 1. Prepare Conv1 im2col
        prepare_im2col1(img_input);

        // --- SOFTWARE INFERENCE BENCHMARK ---
        uint32_t sw_c_start = neorv32_cpu_csr_read(CSR_MCYCLE);
        // Conv1 SW (144 x 32 x 8)
        gemm_sw(col1_buf, W_CONV1, out1_buf_sw, 144, 32, 8);
        for (int i = 0; i < 144 * 8; i++) {
            int32_t v = (out1_buf_sw[i] + B_CONV1[i % 8]) >> 6;
            if (v < 0) v = 0; else if (v > 127) v = 127;
            feat1_act[i] = (int8_t)v;
        }
        prepare_im2col2(feat1_act);
        // Conv2 SW (25 x 72 x 16)
        gemm_sw(col2_buf, W_CONV2, out2_buf_sw, 25, 72, 16);
        for (int r = 0; r < 25; r++) {
            for (int ch = 0; ch < 16; ch++) {
                int32_t v = (out2_buf_sw[r * 16 + ch] + B_CONV2[ch]) >> 6;
                if (v < 0) v = 0; else if (v > 127) v = 127;
                feat2_act[r * 16 + ch] = (int8_t)v;
            }
        }
        // FC SW (1 x 400 x 40)
        for (int i = 0; i < 400; i++) fc_in[i] = feat2_act[i];
        for (int i = 400; i < 8 * 400; i++) fc_in[i] = 0;
        gemm_sw(fc_in, W_FC, fc_out_sw, 1, 400, 40);
        uint32_t sw_cycles = neorv32_cpu_csr_read(CSR_MCYCLE) - sw_c_start;

        // --- HARDWARE SYSTOLIC ACCELERATOR INFERENCE ---
        neorv32_gpio_port_set(0x0F);
        uint32_t hw_c_start = neorv32_cpu_csr_read(CSR_MCYCLE);

        // Conv1 HW
        gemmrv_mat mat_col1 = GEMMRV_MAT(col1_buf, 144, 32, 32);
        gemmrv_mat mat_w1   = GEMMRV_MAT(W_CONV1, 32, 8, 8);
        gemmrv_mat_out mat_out1 = GEMMRV_MAT_OUT(out1_buf_hw, 144, 8, 8);
        gemmrv_mult(&mat_col1, &mat_w1, &mat_out1);

        for (int i = 0; i < 144 * 8; i++) {
            int32_t v = (out1_buf_hw[i] + B_CONV1[i % 8]) >> 6;
            if (v < 0) v = 0; else if (v > 127) v = 127;
            feat1_act[i] = (int8_t)v;
        }

        prepare_im2col2(feat1_act);

        // Conv2 HW
        gemmrv_mat mat_col2 = GEMMRV_MAT(col2_buf, 32, 72, 72);
        gemmrv_mat mat_w2   = GEMMRV_MAT(W_CONV2, 72, 16, 16);
        gemmrv_mat_out mat_out2 = GEMMRV_MAT_OUT(out2_buf_hw, 32, 16, 16);
        gemmrv_mult(&mat_col2, &mat_w2, &mat_out2);

        for (int r = 0; r < 25; r++) {
            for (int ch = 0; ch < 16; ch++) {
                int32_t v = (out2_buf_hw[r * 16 + ch] + B_CONV2[ch]) >> 6;
                if (v < 0) v = 0; else if (v > 127) v = 127;
                feat2_act[r * 16 + ch] = (int8_t)v;
            }
        }

        // FC HW
        for (int i = 0; i < 400; i++) fc_in[i] = feat2_act[i];
        for (int i = 400; i < 8 * 400; i++) fc_in[i] = 0;
        gemmrv_mat mat_fcin = GEMMRV_MAT(fc_in, 8, 400, 400);
        gemmrv_mat mat_wfc  = GEMMRV_MAT(W_FC, 400, 40, 40);
        gemmrv_mat_out mat_outfc = GEMMRV_MAT_OUT(fc_out_hw, 8, 40, 40);
        gemmrv_mult(&mat_fcin, &mat_wfc, &mat_outfc);

        uint32_t hw_cycles = neorv32_cpu_csr_read(CSR_MCYCLE) - hw_c_start;

        // Post-process final logits
        int best_class = 0;
        int32_t max_logit = -2000000000;
        for (int c = 0; c < CNN_NUM_CLASSES; c++) {
            int32_t l = fc_out_hw[c] + B_FC[c];
            if (l > max_logit) {
                max_logit = l;
                best_class = c;
            }
        }

        char pred_char = CNN_CLASSES[best_class];
        uint32_t speedup_x10 = (sw_cycles * 10) / (hw_cycles > 0 ? hw_cycles : 1);

        // Emit JSON result back over UART
        neorv32_uart0_printf("\nRESULT:{\"pred\":\"%c\",\"class_idx\":%d,\"sw_cycles\":%u,\"hw_cycles\":%u,\"speedup\":\"%u.%ux\"}\n",
                             pred_char, best_class, sw_cycles, hw_cycles, speedup_x10 / 10, speedup_x10 % 10);
    }

    return 0;
}
