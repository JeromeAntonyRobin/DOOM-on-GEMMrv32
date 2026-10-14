#include "neorv32.h"
#include <stdint.h>
extern const int8_t conv1_weight[];
extern int8_t X_UNROLLED[];
void debug_dump() {
    int k = 0;
    int m = 0;
    int n = 0;
    neorv32_uart0_printf("\n=== DEBUG DUMP ===\n");
    neorv32_uart0_printf("W tile (k=0, m=0):\n");
    for(int r=0; r<8; r++) {
        for(int c=0; c<8; c++) {
            neorv32_uart0_printf("%d ", conv1_weight[(m+r)*75 + (k+c)]);
        }
        neorv32_uart0_printf("\n");
    }
    neorv32_uart0_printf("X tile (k=0, n=0):\n");
    for(int r=0; r<8; r++) {
        for(int c=0; c<8; c++) {
            neorv32_uart0_printf("%d ", X_UNROLLED[(k+r)*784 + (n+c)]);
        }
        neorv32_uart0_printf("\n");
    }
}
