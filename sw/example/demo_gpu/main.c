#include <neorv32.h>
#include <stdint.h>
#include <stdbool.h>
#include "../gemm_lib/gemmrv_gpu.h"
#include "../gemm_lib/gemmrv_hw.h"

// 12-bit RGB Color definitions
#define COLOR_BLACK     0x000
#define COLOR_BLUE      0x00F
#define COLOR_GREEN     0x0F0
#define COLOR_CYAN      0x0FF
#define COLOR_RED       0xF00
#define COLOR_MAGENTA   0xF0F
#define COLOR_YELLOW    0xFF0
#define COLOR_WHITE     0xFFF
#define COLOR_DARK_GRAY 0x444
#define COLOR_LIGHT_GRAY 0xAAA

static void setup_palette(void) {
    vga_set_palette_entry(0, COLOR_BLACK);
    vga_set_palette_entry(1, COLOR_BLUE);
    vga_set_palette_entry(2, COLOR_GREEN);
    vga_set_palette_entry(3, COLOR_CYAN);
    vga_set_palette_entry(4, COLOR_RED);
    vga_set_palette_entry(5, COLOR_MAGENTA);
    vga_set_palette_entry(6, COLOR_YELLOW);
    vga_set_palette_entry(7, COLOR_WHITE);
    vga_set_palette_entry(8, COLOR_DARK_GRAY);
    vga_set_palette_entry(9, COLOR_LIGHT_GRAY);
}

// Draw test patterns: Color bars across the screen
static void draw_test_pattern(void) {
    vga_clear(0); // Black

    // Draw 8 horizontal color bars
    int bar_height = VGA_HEIGHT / 8;
    for (int b = 0; b < 8; b++) {
        for (int y = b * bar_height; y < (b + 1) * bar_height; y++) {
            vga_draw_hline(0, y, VGA_WIDTH, (uint8_t)b);
        }
    }

    // Border
    vga_draw_hline(0, 0, VGA_WIDTH, 7);
    vga_draw_hline(0, VGA_HEIGHT - 1, VGA_WIDTH, 7);
    vga_draw_vline(0, 0, VGA_HEIGHT, 7);
    vga_draw_vline(VGA_WIDTH - 1, 0, VGA_HEIGHT, 7);
}

int main(void) {
    // Initialize NEORV32 UART console
    neorv32_uart0_setup(19200, 0);

    neorv32_uart0_printf("\n============================================\n");
    neorv32_uart0_printf("   GEMMrv32 GPU & Standalone Workstation    \n");
    neorv32_uart0_printf("   VGA Resolution: 160x120 (4x to 640x480) \n");
    neorv32_uart0_printf("   PS/2 Keyboard Interface Active          \n");
    neorv32_uart0_printf("============================================\n\n");

    // Initialize palette and video display
    setup_palette();
    draw_test_pattern();

    neorv32_uart0_printf("[VGA] Display initialized. Color bars rendered.\n");
    neorv32_uart0_printf("[PS/2] Listening for keyboard input...\n");

    // Interactive Loop: Echo keyboard events to UART and toggle pixels/colors on VGA
    uint8_t current_color = 1;
    int cursor_x = 10;
    int cursor_y = 10;

    while (1) {
        if (ps2_has_key()) {
            uint8_t sc = ps2_get_scancode();
            char ch = ps2_scancode_to_ascii(sc);

            neorv32_uart0_printf("[KEYBOARD] Scancode: 0x%02x", sc);
            if (ch >= 32 && ch <= 126) {
                neorv32_uart0_printf(" ('%c')", ch);
            }
            neorv32_uart0_printf("\n");

            // Change color or draw
            current_color = (current_color % 7) + 1;
            
            // Draw a 4x4 block at cursor
            for (int dy = 0; dy < 4; dy++) {
                vga_draw_hline(cursor_x, cursor_y + dy, 4, current_color);
            }

            cursor_x += 6;
            if (cursor_x >= (VGA_WIDTH - 10)) {
                cursor_x = 10;
                cursor_y += 6;
                if (cursor_y >= (VGA_HEIGHT - 10)) {
                    cursor_y = 10;
                    draw_test_pattern();
                }
            }
        }
    }

    return 0;
}
