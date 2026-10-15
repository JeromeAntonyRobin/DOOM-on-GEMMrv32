#include "gemmrv_gpu.h"
#include "gemmrv_hw.h"

// -----------------------------------------------------------------------------
// Video Controller Routines
// -----------------------------------------------------------------------------

void vga_set_palette(const uint16_t *palette_table) {
    if (!palette_table) return;
    volatile uint32_t *pal = (volatile uint32_t *)VGA_PALETTE_BASE;
    for (int i = 0; i < 256; i++) {
        pal[i] = palette_table[i] & 0x0FFF;
    }
}

void vga_clear(uint8_t color_idx) {
    volatile uint32_t *vram32 = (volatile uint32_t *)VGA_VRAM_BASE;
    uint32_t fill_val = ((uint32_t)color_idx) |
                        ((uint32_t)color_idx << 8) |
                        ((uint32_t)color_idx << 16) |
                        ((uint32_t)color_idx << 24);
    
    // 19200 bytes = 4800 32-bit words
    for (int i = 0; i < (VGA_NUM_PIXELS / 4); i++) {
        vram32[i] = fill_val;
    }
}

void vga_draw_hline(int x, int y, int length, uint8_t color_idx) {
    if (y < 0 || y >= VGA_HEIGHT) return;
    if (x < 0) { length += x; x = 0; }
    if (x + length > VGA_WIDTH) length = VGA_WIDTH - x;
    if (length <= 0) return;

    volatile uint8_t *line = (volatile uint8_t *)(VGA_VRAM_BASE + (y * VGA_WIDTH + x));
    for (int i = 0; i < length; i++) {
        line[i] = color_idx;
    }
}

void vga_draw_vline(int x, int y, int length, uint8_t color_idx) {
    if (x < 0 || x >= VGA_WIDTH) return;
    if (y < 0) { length += y; y = 0; }
    if (y + length > VGA_HEIGHT) length = VGA_HEIGHT - y;
    if (length <= 0) return;

    volatile uint8_t *ptr = (volatile uint8_t *)(VGA_VRAM_BASE + (y * VGA_WIDTH + x));
    for (int i = 0; i < length; i++) {
        *ptr = color_idx;
        ptr += VGA_WIDTH;
    }
}

void vga_blit_rect(int x, int y, int w, int h, const uint8_t *src_pixels) {
    if (!src_pixels) return;
    for (int r = 0; r < h; r++) {
        int dst_y = y + r;
        if (dst_y < 0 || dst_y >= VGA_HEIGHT) continue;
        
        volatile uint8_t *dst = (volatile uint8_t *)(VGA_VRAM_BASE + (dst_y * VGA_WIDTH + x));
        const uint8_t *src = src_pixels + (r * w);
        for (int c = 0; c < w; c++) {
            int dst_x = x + c;
            if (dst_x >= 0 && dst_x < VGA_WIDTH) {
                dst[c] = src[c];
            }
        }
    }
}

// -----------------------------------------------------------------------------
// GEMM GPU Accelerated Rendering Engine
// -----------------------------------------------------------------------------

void gpu_clear_screen(uint8_t color_idx) {
    vga_clear(color_idx);
}

void gpu_draw_wall_column(int x, int ytop, int ybottom, uint32_t tex_step, const uint8_t *texture, int tex_height) {
    if (x < 0 || x >= VGA_WIDTH) return;
    if (ytop < 0) ytop = 0;
    if (ybottom >= VGA_HEIGHT) ybottom = VGA_HEIGHT - 1;
    if (ytop > ybottom) return;

    volatile uint8_t *dst = (volatile uint8_t *)(VGA_VRAM_BASE + (ytop * VGA_WIDTH + x));
    uint32_t tex_coord = 0;
    
    for (int y = ytop; y <= ybottom; y++) {
        int tex_y = (tex_coord >> 16) % tex_height;
        *dst = texture[tex_y];
        dst += VGA_WIDTH;
        tex_coord += tex_step;
    }
}

void gpu_transform_vertices_8x(const int8_t *in_verts_8x4, const int8_t *matrix_4x4, int32_t *out_verts_8x4) {
    // Uses the 8x8 systolic array coprocessor:
    // Padded to 8x8 blocks:
    // Matrix A: 8 vertices x 4 dimensions -> padded to 8x8
    // Matrix B: 4x4 transformation -> padded to 8x8
    int8_t pad_A[64] __attribute__((aligned(32))) = {0};
    int8_t pad_B[64] __attribute__((aligned(32))) = {0};
    int32_t pad_C[64] __attribute__((aligned(32))) = {0};

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 4; c++) {
            pad_A[r * 8 + c] = in_verts_8x4[r * 4 + c];
        }
    }

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            pad_B[r * 8 + c] = matrix_4x4[r * 4 + c];
        }
    }

    gemmrv_mat A = GEMMRV_MAT(pad_A, 8, 8, 8);
    gemmrv_mat B = GEMMRV_MAT(pad_B, 8, 8, 8);
    gemmrv_mat_out C = GEMMRV_MAT_OUT(pad_C, 8, 8, 8);

    gemmrv_mult(&A, &B, &C);

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 4; c++) {
            out_verts_8x4[r * 4 + c] = pad_C[r * 8 + c];
        }
    }
}

// -----------------------------------------------------------------------------
// PS/2 Keyboard Driver
// -----------------------------------------------------------------------------

uint8_t ps2_wait_scancode(void) {
    while (!ps2_has_key());
    return ps2_get_scancode();
}

char ps2_scancode_to_ascii(uint8_t scancode) {
    // Basic lookup for standard US QWERTY Set-2 make codes
    switch (scancode) {
        case 0x1C: return 'a';
        case 0x32: return 'b';
        case 0x21: return 'c';
        case 0x23: return 'd';
        case 0x24: return 'e';
        case 0x2B: return 'f';
        case 0x34: return 'g';
        case 0x33: return 'h';
        case 0x43: return 'i';
        case 0x3B: return 'j';
        case 0x42: return 'k';
        case 0x4B: return 'l';
        case 0x3A: return 'm';
        case 0x31: return 'n';
        case 0x44: return 'o';
        case 0x4D: return 'p';
        case 0x15: return 'q';
        case 0x2D: return 'r';
        case 0x1B: return 's';
        case 0x2C: return 't';
        case 0x3C: return 'u';
        case 0x2A: return 'v';
        case 0x1D: return 'w';
        case 0x22: return 'x';
        case 0x35: return 'y';
        case 0x1A: return 'z';
        case 0x45: return '0';
        case 0x16: return '1';
        case 0x1E: return '2';
        case 0x26: return '3';
        case 0x25: return '4';
        case 0x2E: return '5';
        case 0x36: return '6';
        case 0x3D: return '7';
        case 0x3E: return '8';
        case 0x46: return '9';
        case 0x29: return ' ';
        case 0x5A: return '\n'; // Enter
        case 0x66: return '\b'; // Backspace
        default:   return 0;
    }
}
