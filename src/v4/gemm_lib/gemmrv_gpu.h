#ifndef GEMMRV_GPU_H
#define GEMMRV_GPU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==============================================================================
// Memory-Mapped I/O Base Addresses
// ==============================================================================
#ifndef VGA_VRAM_BASE
#define VGA_VRAM_BASE       0x90000000UL  // Framebuffer base address
#endif
#ifndef VGA_PALETTE_BASE
#define VGA_PALETTE_BASE    0x90010000UL  // 256 x 12-bit RGB Color Lookup Table
#endif
#define VGA_CTRL_BASE       0x90010400UL  // Status Register ([0]: in_vblank)

#define PS2_DATA_REG        0x90020000UL  // Keyboard Scancode FIFO read
#define PS2_STAT_REG        0x90020004UL  // Keyboard Status ([0]: data_ready, [1]: full)

// Display characteristics
#define VGA_WIDTH           160
#define VGA_HEIGHT          120
#define VGA_NUM_PIXELS      (VGA_WIDTH * VGA_HEIGHT)

// ==============================================================================
// Video & Display Controller API
// ==============================================================================

// Wait for next vertical blanking interval (tear-free rendering)
static inline void vga_wait_vsync(void) {
    volatile uint32_t *ctrl = (volatile uint32_t *)VGA_CTRL_BASE;
    while (!(*ctrl & 1)); // wait until inside vblank
    while (*ctrl & 1);  // wait until exit vblank
}

// Check if currently inside vertical blanking
static inline bool vga_is_vblank(void) {
    return (*((volatile uint32_t *)VGA_CTRL_BASE) & 1) != 0;
}

// Program a single 12-bit color in the hardware palette (0..255)
// rgb12 format: [11:8] = Red (0..15), [7:4] = Green (0..15), [3:0] = Blue (0..15)
static inline void vga_set_palette_entry(uint8_t index, uint16_t rgb12) {
    volatile uint32_t *pal = (volatile uint32_t *)(VGA_PALETTE_BASE + ((uint32_t)index << 2));
    *pal = rgb12 & 0x0FFF;
}

// Load full 256-entry 12-bit color palette
void vga_set_palette(const uint16_t *palette_table);

// Clear screen with a solid palette color index
void vga_clear(uint8_t color_idx);

// Put a single pixel
static inline void vga_put_pixel(int x, int y, uint8_t color_idx) {
    if ((unsigned)x < VGA_WIDTH && (unsigned)y < VGA_HEIGHT) {
        volatile uint8_t *vram = (volatile uint8_t *)(VGA_VRAM_BASE + (y * VGA_WIDTH + x));
        *vram = color_idx;
    }
}

// Draw horizontal line
void vga_draw_hline(int x, int y, int length, uint8_t color_idx);

// Draw vertical line
void vga_draw_vline(int x, int y, int length, uint8_t color_idx);

// Fast rectangular blit to VRAM
void vga_blit_rect(int x, int y, int w, int h, const uint8_t *src_pixels);

// ==============================================================================
// GEMM GPU Accelerated Rendering Engine
// ==============================================================================

// Fast hardware clear using 32-bit burst words
void gpu_clear_screen(uint8_t color_idx);

// Hardware-assisted Doom texture column renderer
// Computes and draws vertical wall column from scaled texture
void gpu_draw_wall_column(int x, int ytop, int ybottom, uint32_t tex_step, const uint8_t *texture, int tex_height);

// Batch 3D vertex transformation: V_out (8x4) = V_in (8x4) * TransformMatrix (4x4)
// Accelerated by the 8x8 systolic array coprocessor
void gpu_transform_vertices_8x(const int8_t *in_verts_8x4, const int8_t *matrix_4x4, int32_t *out_verts_8x4);

// ==============================================================================
// PS/2 Keyboard Controller API
// ==============================================================================

// Returns true if a key scancode is pending in the FIFO
static inline bool ps2_has_key(void) {
    return (*((volatile uint32_t *)PS2_STAT_REG) & 1) != 0;
}

// Reads one raw scancode byte from the keyboard FIFO
static inline uint8_t ps2_get_scancode(void) {
    return (uint8_t)(*((volatile uint32_t *)PS2_DATA_REG) & 0xFF);
}

// Polls until a key event arrives
uint8_t ps2_wait_scancode(void);

// Convert single Set-2 PS/2 make scancode to ASCII character (returns 0 if non-printable)
char ps2_scancode_to_ascii(uint8_t scancode);

#ifdef __cplusplus
}
#endif

#endif // GEMMRV_GPU_H
