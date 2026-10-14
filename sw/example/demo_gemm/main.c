#include <stdint.h>
#include <neorv32.h>
#include "../gemm_lib/gemmrv_hw.h"

#define MATRIX_SIZE 8

// Statically placed in internal DMEM (BRAM)
int8_t raw_A[MATRIX_SIZE * MATRIX_SIZE] __attribute__((aligned(32)));
int8_t raw_B[MATRIX_SIZE * MATRIX_SIZE] __attribute__((aligned(32)));
int32_t raw_C_hw[MATRIX_SIZE * MATRIX_SIZE] __attribute__((aligned(32)));
int32_t raw_C_sw[MATRIX_SIZE * MATRIX_SIZE] __attribute__((aligned(32)));

void gemm_sw_basic(const int8_t* A, const int8_t* B, int32_t* C, int M, int K, int N) {
    for(int m=0; m<M; m++) {
        for(int n=0; n<N; n++) {
            int32_t acc = 0;
            for(int k=0; k<K; k++) {
                acc += (int32_t)A[m * K + k] * (int32_t)B[k * N + n];
            }
            C[m * N + n] = acc;
        }
    }
}

int main() {
    // 1. Immediately turn on LEDs as a heart-beat that CPU has booted
    neorv32_gpio_port_set(0x01);

    // 2. Setup UART
    neorv32_uart0_setup(19200, 0);

    // 3. Blink LED to indicate UART configured
    neorv32_gpio_port_set(0x03);

    while (1) {
        neorv32_uart0_printf("\n\n===========================================\n");
        neorv32_uart0_printf("=== GEMMrv32 LIVE HARDWARE ACCELERATOR ===\n");
        neorv32_uart0_printf("Matrix Dimension: %d x %d\n", MATRIX_SIZE, MATRIX_SIZE);
        neorv32_uart0_printf("Systolic Array:   8x8 (64 MACs)\n");
        neorv32_uart0_printf("Memory Bus:       256-bit DMA\n");
        neorv32_uart0_printf("===========================================\n");

        neorv32_gpio_port_set(0x07);

        // Initialize with test data
        for(int i=0; i<MATRIX_SIZE * MATRIX_SIZE; i++) {
            raw_A[i] = (i % 7) - 3;
            raw_B[i] = (i % 5) - 2;
            raw_C_hw[i] = 0;
            raw_C_sw[i] = 0;
        }

        gemmrv_mat A = GEMMRV_MAT(raw_A, MATRIX_SIZE, MATRIX_SIZE, MATRIX_SIZE);
        gemmrv_mat B = GEMMRV_MAT(raw_B, MATRIX_SIZE, MATRIX_SIZE, MATRIX_SIZE);
        gemmrv_mat_out C = GEMMRV_MAT_OUT(raw_C_hw, MATRIX_SIZE, MATRIX_SIZE, MATRIX_SIZE);

        neorv32_uart0_printf("\n[HW] Running 8x8 Systolic Hardware GEMM...\n");
        neorv32_gpio_port_set(0x0F);
        uint32_t hw_start = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemmrv_mult(&A, &B, &C);
        uint32_t hw_end = neorv32_cpu_csr_read(CSR_MCYCLE);
        uint32_t hw_cycles = hw_end - hw_start;
        neorv32_uart0_printf("[HW] Finished in %u cycles.\n", hw_cycles);

        neorv32_uart0_printf("\n[SW] Running RISC-V Scalar CPU Software GEMM...\n");
        neorv32_gpio_port_set(0x1F);
        uint32_t sw_start = neorv32_cpu_csr_read(CSR_MCYCLE);
        gemm_sw_basic(raw_A, raw_B, raw_C_sw, MATRIX_SIZE, MATRIX_SIZE, MATRIX_SIZE);
        uint32_t sw_end = neorv32_cpu_csr_read(CSR_MCYCLE);
        uint32_t sw_cycles = sw_end - sw_start;
        neorv32_uart0_printf("[SW] Finished in %u cycles.\n", sw_cycles);

        int errors = 0;
        for(int i=0; i<MATRIX_SIZE * MATRIX_SIZE; i++) {
            if(raw_C_hw[i] != raw_C_sw[i]) {
                errors++;
            }
        }

        neorv32_uart0_printf("\n--- VERIFICATION & COMPARISON ---\n");
        neorv32_uart0_printf("Mathematical Discrepancies: %d\n", errors);
        neorv32_uart0_printf("Hardware Execution Cycles:  %u\n", hw_cycles);
        neorv32_uart0_printf("Software Execution Cycles:  %u\n", sw_cycles);
        
        uint32_t speedup_int = sw_cycles / hw_cycles;
        uint32_t speedup_frac = ((sw_cycles % hw_cycles) * 100) / hw_cycles;
        neorv32_uart0_printf("Hardware GEMM Speedup:      %u.%02ux\n", speedup_int, speedup_frac);
        neorv32_uart0_printf("===========================================\n");

        // Flash all LEDs in alternating pattern to signal successful pass
        uint32_t clk = neorv32_sysinfo_get_clk();
        for(int b=0; b<6; b++) {
            neorv32_gpio_port_set(0xAA);
            neorv32_aux_delay_ms(clk, 250);
            neorv32_gpio_port_set(0x55);
            neorv32_aux_delay_ms(clk, 250);
        }
    }

    return 0;
}
