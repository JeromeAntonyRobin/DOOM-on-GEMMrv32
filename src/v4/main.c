// ==============================================================================
// GEMMrv32 DOOM v3: Native 320x200 @ 4bpp Authentic BSP Engine
// Core: NEORV32 RV32IM @ 50 MHz | Coprocessor: 8x8 Systolic GEMM Array
// Architecture: BSP Tree, Linedefs, Multi-Height Sectors, Mobjs & 35 Hz Ticker
// ==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <neorv32.h>
#include "doom_defs.h"
#include "doom_map.h"
#include "doom_assets_authentic.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "p_mobj.h"
#include "gemm_lib/gemmrv_gpu.h"
#include <string.h>

#define VIEWPORT_H         168 // 3D world takes top 168 scanlines (0..167)
#define STATUS_BAR_H        32 // Bottom 32 scanlines (168..199)

// Game Stats
int player_health = 100;
int player_ammo   = 50;
int player_score  = 0;

static uint32_t frame_count = 0;
static uint32_t gemm_batches = 0;
static bool show_minimap = true;
static int muzzle_flash = 0;
static int walk_bob = 0;

// Sine and Cosine LUT in 16.16 fixed-point (64 entries for 360 degrees)
static const fixed_t sin_table[64] = {
    0, 6423, 12785, 19024, 25079, 30893, 36409, 41575,
    46340, 50660, 54491, 57797, 60547, 62714, 64276, 65219,
    65536, 65219, 64276, 62714, 60547, 57797, 54491, 50660,
    46340, 41575, 36409, 30893, 25079, 19024, 12785, 6423,
    0, -6423, -12785, -19024, -25079, -30893, -36409, -41575,
    -46340, -50660, -54491, -57797, -60547, -62714, -64276, -65219,
    -65536, -65219, -64276, -62714, -60547, -57797, -54491, -50660,
    -46340, -41575, -36409, -30893, -25079, -19024, -12785, -6423
};

static int player_angle_idx = 16; // Facing North (90 degrees)

static inline fixed_t get_sin(int idx) { return sin_table[idx & 63]; }
static inline fixed_t get_cos(int idx) { return sin_table[(idx + 16) & 63]; }

// ==============================================================================
// Render Status Bar HUD (Bottom 32 scanlines: y = 168..199)
// ==============================================================================
// ==============================================================================
// Render Status Bar HUD (Bottom 32 scanlines: y = 168..199)
// ==============================================================================
static void render_status_bar(volatile uint8_t *vram) {
    // 1. Draw Industrial Grey Beveled Plate using authentic DOOM 2 grey ramp (palette indices 96..111)
    for (int y = VIEWPORT_H; y < SCREEN_H; y++) {
        uint8_t line_col = 104; // Medium grey
        if (y == VIEWPORT_H)          line_col = 100; // Top shadow line
        else if (y == VIEWPORT_H + 1) line_col = 108; // Bevel highlight
        else if (y == SCREEN_H - 1)   line_col = 96;  // Bottom dark groove

        for (int x = 0; x < SCREEN_W; x++) {
            vram[y * SCREEN_W + x] = line_col;
        }
    }

    // 2. Draw Authentic Doomguy Face in Center (STFST00 / STFEVL0)
    const doom_patch_t *face = (muzzle_flash > 0) ? &patch_stfevl0 : &patch_stfst00;
    int face_x = (SCREEN_W - face->width) / 2;
    int face_y = VIEWPORT_H + 1;
    draw_authentic_patch(vram, face, face_x, face_y);

    // 3. Ammo Bar on Left (x = 20..84, y = 180..187)
    int ammo_fill = (player_ammo * 64) / 50;
    for (int y = VIEWPORT_H + 12; y <= VIEWPORT_H + 19; y++) {
        for (int x = 20; x <= 84; x++) {
            uint8_t col = (x - 20 <= ammo_fill) ? 224 : 0; // Gold amber ammo bar (DOOM 2 index 224)
            vram[y * SCREEN_W + x] = col;
        }
    }

    // 4. Health Bar on Right (x = 236..300, y = 180..187)
    int health_fill = (player_health * 64) / 100;
    for (int y = VIEWPORT_H + 12; y <= VIEWPORT_H + 19; y++) {
        for (int x = 236; x <= 300; x++) {
            uint8_t col = (x - 236 <= health_fill) ? 176 : 0; // Blood red health bar (DOOM 2 index 176)
            vram[y * SCREEN_W + x] = col;
        }
    }
}

// ==============================================================================
// Render First-Person Authentic Super Shotgun Weapon Overlay
// ==============================================================================
static void render_weapon(volatile uint8_t *vram) {
    const doom_patch_t *wpn = &patch_sht2a0;
    int wpn_x = (SCREEN_W - wpn->width) / 2;
    int wpn_y = VIEWPORT_H - wpn->height + (walk_bob ? 3 : 0);
    draw_authentic_patch(vram, wpn, wpn_x, wpn_y);
}

// ==============================================================================
// Render 2D Minimap Overlay in Top-Left Corner
// ==============================================================================
static void render_minimap(volatile uint8_t *vram) {
    if (!show_minimap) return;

    // Draw lines of the level
    for (size_t i = 0; i < NUM_LINES; i++) {
        line_t *line = &map_lines[i];
        int x1 = (FROM_FIXED(line->v1->x) + 400) / 16 + 4;
        int y1 = (FROM_FIXED(line->v1->y) + 250) / 16 + 4;
        int x2 = (FROM_FIXED(line->v2->x) + 400) / 16 + 4;
        int y2 = (FROM_FIXED(line->v2->y) + 250) / 16 + 4;

        int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
        int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
        int sx = (x1 < x2) ? 1 : -1;
        int sy = (y1 < y2) ? 1 : -1;
        int err = dx - dy;

        while (1) {
            uint8_t col = (line->flags & ML_TWOSIDED) ? 224 : 112; // Amber/grey
            put_pixel(vram, x1, y1, col);
            if (x1 == x2 && y1 == y2) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x1 += sx; }
            if (e2 < dx)  { err += dx; y1 += sy; }
        }
    }

    // Draw player position
    int px = (FROM_FIXED(viewx) + 400) / 16 + 4;
    int py = (FROM_FIXED(viewy) + 250) / 16 + 4;
    put_pixel(vram, px, py, 4); // White dot
    int dir_x = px + FROM_FIXED(viewsin * 4);
    int dir_y = py + FROM_FIXED(viewcos * 4);
    put_pixel(vram, dir_x, dir_y, 176); // Red tick
}

// ==============================================================================
// 3D Frame Rendering Pipeline: BSP Walk, Linedefs, Mobjs, Weapon & HUD
// ==============================================================================
static void render_doom_v3_frame(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    // 0. Synchronize with VGA Vertical Blanking to prevent screen tearing
    wait_for_vblank();

    // 1. Reset Clipping and Occlusion Buffers
    R_ClearDrawSegs();
    R_ClearClipSegs();

    // 2. Set Camera Context from Player Mobj
    if (player_mobj) {
        viewx = player_mobj->x;
        viewy = player_mobj->y;
        viewz = player_mobj->z + TO_FIXED(41); // Eye height = 41 units
    }
    viewcos = get_cos(player_angle_idx);
    viewsin = get_sin(player_angle_idx);

    // 3. Front-to-Back Recursive BSP Tree Traversal & Linedef Rasterization
    R_RenderBSPNode(0);

    // 4. Render Dynamic 2.5D Mobjs (Imps, Fireballs, Barrels) via GEMM Coprocessor
    R_DrawMasked();
    gemm_batches++;

    // 5. Render Authentic First-Person Super Shotgun
    render_weapon(vram);

    // 6. Render Authentic Status Bar HUD
    render_status_bar(vram);

    // 7. Render Minimap Overlay
    render_minimap(vram);

    frame_count++;
}

// ==============================================================================
// Player Input & Collision Coordinator
// ==============================================================================
static void handle_input(char cmd) {
    fixed_t move_speed = TO_FIXED(12);

    switch (cmd) {
        case 'w': case 'W': // Forward
            if (player_mobj) {
                player_mobj->x += FIXED_MUL(viewsin, move_speed);
                player_mobj->y += FIXED_MUL(viewcos, move_speed);
                walk_bob = !walk_bob;
            }
            break;
        case 's': case 'S': // Backward
            if (player_mobj) {
                player_mobj->x -= FIXED_MUL(viewsin, move_speed);
                player_mobj->y -= FIXED_MUL(viewcos, move_speed);
                walk_bob = !walk_bob;
            }
            break;
        case 'a': case 'A': // Turn Left
            player_angle_idx = (player_angle_idx - 1) & 63;
            break;
        case 'd': case 'D': // Turn Right
            player_angle_idx = (player_angle_idx + 1) & 63;
            break;
        case ' ': case 'f': case 'F': // FIRE SUPER SHOTGUN!
            if (player_ammo > 0) {
                player_ammo -= 2;
                muzzle_flash = 3; // Trigger double muzzle flash
                neorv32_uart0_printf("[DOOM 2] BOOM! Super Shotgun fired 20 pellets! Ammo: %d\n", player_ammo);

                // Inflict massive buckshot damage across 20 pellets in wide cone
                for (int i = 0; i < num_mobjs; i++) {
                    mobj_t *mo = &mobjs[i];
                    if (mo->type == MT_IMP && mo->health > 0) {
                        int d_x = FROM_FIXED(mo->x - viewx);
                        int d_y = FROM_FIXED(mo->y - viewy);
                        if (d_x * d_x + d_y * d_y < 500 * 500) {
                            mo->health -= 120; // Massive Super Shotgun blast!
                            if (mo->health <= 0) {
                                mo->state = S_IMP_DIE1;
                                neorv32_uart0_printf("[DOOM 2] Imp blasted to pieces!\n");
                            } else {
                                mo->state = S_IMP_PAIN;
                                neorv32_uart0_printf("[DOOM 2] Imp wounded! Remaining health: %d\n", mo->health);
                            }
                        }
                    }
                }
            }
            break;
        case 'm': case 'M': // Toggle Minimap
            show_minimap = !show_minimap;
            break;
        case 'r': case 'R': // Respawn
            if (player_mobj) {
                player_mobj->x = TO_FIXED(0);
                player_mobj->y = TO_FIXED(-100);
                player_mobj->z = TO_FIXED(0);
                player_angle_idx = 16;
                player_health = 100;
                player_ammo = 50;
                neorv32_uart0_printf("[DOOM 2] Player respawned at (0, -100)\n");
            }
            break;
        default:
            break;
    }
}

// ==============================================================================
// Main Entry Point
// ==============================================================================
int main(void) {
    neorv32_uart0_setup(19200, 0);

    neorv32_uart0_printf("\n======================================================\n");
    neorv32_uart0_printf("   GEMMrv32 DOOM 2: Native 320x200 @ 8bpp Mode 13h     \n");
    neorv32_uart0_printf("   Architecture: 256-Color VGA, BSP Tree, Super Shotgun\n");
    neorv32_uart0_printf("   Controls: W=Forward, S=Back, A=Left, D=Right       \n");
    neorv32_uart0_printf("             SPACE=Fire Super Shotgun, M=Map, R=Reset \n");
    neorv32_uart0_printf("======================================================\n\n");

    // 0. Clear 64,000-byte VRAM to black
    memset((void *)VGA_VRAM_BASE, 0, VRAM_BYTES);

    // 1. Initialize 256-color 12-bit RGB Palette from authentic DOOM 2 PLAYPAL
    setup_authentic_doom2_palette();
    neorv32_uart0_printf("[ASSETS] Authentic DOOM 2 256-color PLAYPAL, STARTAN3 & Super Shotgun loaded!\n");

    // 3. Initialize Map Geometry & Mobjs
    P_InitMobjs();
    neorv32_uart0_printf("[MAP] DOOM 2 MAP01 multi-height BSP world & entities spawned!\n");

    // 4. Render Initial Native 320x200 Frame
    render_doom_v3_frame();
    neorv32_uart0_printf("[DOOM 2] First Native 320x200 Mode 13h frame rendered successfully!\n");

    int last_report = 0;

    while (1) {
        bool state_changed = false;

        // Poll UART0 (PC Keyboard)
        if (neorv32_uart0_available()) {
            char cmd = (char)neorv32_uart0_getc();
            handle_input(cmd);
            state_changed = true;
        }

        // Poll PS/2 Keyboard Port
        if (*((volatile uint32_t *)PS2_STAT_REG) & 1) {
            uint8_t sc = (uint8_t)(*((volatile uint32_t *)PS2_DATA_REG) & 0xFF);
            char ch = ps2_scancode_to_ascii(sc);
            if (ch) {
                handle_input(ch);
                state_changed = true;
            }
        }

        // 35 Hz Ticker: Advance Monster AI, Fireball Projectiles, and Animations
        P_Ticker();

        // Render Frame
        if (state_changed || muzzle_flash > 0) {
            if (muzzle_flash > 0) muzzle_flash--;

            render_doom_v3_frame();

            if (frame_count - last_report >= 10) {
                last_report = frame_count;
                neorv32_uart0_printf("[DOOM v3] Frame: %u | GEMM Batches: %u | Health: %d | Ammo: %d\n",
                    frame_count, gemm_batches, player_health, player_ammo);
            }
        }
    }

    return 0;
}
