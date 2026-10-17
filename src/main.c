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



// Game Stats
int player_health = 100;
int player_ammo   = 50;
int player_score  = 0;

static uint32_t frame_count = 0;
static uint32_t gemm_batches = 0;
static bool show_minimap = true;
static int muzzle_flash = 0;

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

int player_angle_idx = 16; // Facing North (90 degrees)

static inline fixed_t get_sin(int idx) { return sin_table[idx & 63]; }
static inline fixed_t get_cos(int idx) { return sin_table[(idx + 16) & 63]; }

// ==============================================================================
// ==============================================================================
// ==============================================================================
// 4x7 Digital Font Bitmaps for Authentic DOOM Numbers (0-9, %)
// ==============================================================================
static const uint8_t hud_font_digits[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F}, // 2
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}  // 9
};

static const uint8_t hud_font_percent[7] = {
    0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13
};

static const struct { char c; uint16_t g; } hud_glyphs[] = {
    {'A', 0x7BEF}, {'R', 0x7BCE}, {'M', 0x7EBF}, {'O', 0x7B6F},
    {'H', 0x73EF}, {'E', 0x79CF}, {'L', 0x7107}, {'T', 0x1271},
    {'S', 0x4B6E}, {'P', 0x7BE8}, {'F', 0x04BF}, {'W', 0x3F0F},
    {'[', 0x023F}, {']', 0x03F1}
};

static void draw_hud_char_3x5(volatile uint8_t *vram, int x, int y, char ch, uint8_t color) {
    uint16_t glyph = 0;
    for (int i = 0; i < 14; i++) {
        if (hud_glyphs[i].c == ch) { glyph = hud_glyphs[i].g; break; }
    }
    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 5; row++) {
            if ((glyph >> (col * 5 + row)) & 1) {
                vram[(y + row) * SCREEN_W + (x + col)] = color;
            }
        }
    }
}

static void draw_hud_text(volatile uint8_t *vram, int x, int y, const char *str, uint8_t color) {
    while (*str) {
        draw_hud_char_3x5(vram, x, y, *str++, color);
        x += 4;
    }
}

static void draw_hud_pattern(volatile uint8_t *vram, int x, int y, const uint8_t *pat, uint8_t color) {
    for (int r = 0; r < 7; r++) {
        uint8_t row = pat[r];
        volatile uint8_t *p0 = &vram[(y + r * 2) * SCREEN_W + x];
        volatile uint8_t *p1 = p0 + SCREEN_W;
        for (int c = 0; c < 5; c++, p0 += 2, p1 += 2) {
            if (row & (0x10 >> c)) {
                p1[1] = 0; // Drop shadow
                p0[0] = color;
                p0[1] = color;
                p1[0] = color;
            }
        }
    }
}

static void draw_hud_digit(volatile uint8_t *vram, int x, int y, int digit, uint8_t color) {
    if (digit >= 0 && digit <= 9) draw_hud_pattern(vram, x, y, hud_font_digits[digit], color);
}

static int draw_hud_number(volatile uint8_t *vram, int x, int y, int num, uint8_t color) {
    if (num < 0) num = 0;
    if (num > 999) num = 999;
    char buf[4];
    int len = 0;
    int temp = num;
    do {
        buf[len++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    int total_w = 0;
    for (int i = len - 1; i >= 0; i--) {
        draw_hud_digit(vram, x, y, buf[i] - '0', color);
        x += 12;
        total_w += 12;
    }
    return total_w;
}

static void draw_hud_percent(volatile uint8_t *vram, int x, int y, uint8_t color) {
    draw_hud_pattern(vram, x, y, hud_font_percent, color);
}

// ==============================================================================
// Overhauled Authentic DOOM 2 Status Bar HUD (Bottom 32 Scanlines: y = 168..199)
// ==============================================================================
static const int16_t boxes[4][4] = {
    {10, 171, 62, 196},   // AMMO
    {72, 171, 136, 196},  // HEALTH
    {184, 171, 248, 196}, // ARMOR
    {258, 171, 310, 196}  // ARMS
};

static void render_status_bar(volatile uint8_t *vram) {
    // 1. Draw Industrial Grey Beveled Plate
    for (int y = VIEWPORT_H; y < SCREEN_H; y++) {
        uint8_t line_col = 104; // Authentic DOOM dark industrial grey
        if (y == VIEWPORT_H)          line_col = 100; // Top shadow line
        else if (y == VIEWPORT_H + 1) line_col = 108; // Bevel highlight
        else if (y == SCREEN_H - 1)   line_col = 96;  // Bottom dark groove

        for (int x = 0; x < SCREEN_W; x++) {
            vram[y * SCREEN_W + x] = line_col;
        }
    }

    // 2. Beveled Dark Recessed Digital Readout Boxes
    for (int b = 0; b < 4; b++) {
        int x0 = boxes[b][0], y0 = boxes[b][1], x1 = boxes[b][2], y1 = boxes[b][3];
        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                uint8_t col = 0; // Pitch black recessed display
                if (y == y0 || x == x0) col = 96;       // Inset top/left shadow
                else if (y == y1 || x == x1) col = 108; // Inset bottom/right highlight
                vram[y * SCREEN_W + x] = col;
            }
        }
    }

    // 3. Status Bar Labels
    draw_hud_text(vram, 22, 173, "AMMO", 224);     // Gold amber
    draw_hud_text(vram, 88, 173, "HEALTH", 176);   // Blood red
    draw_hud_text(vram, 200, 173, "ARMOR", 112);   // Emerald green
    draw_hud_text(vram, 272, 173, "ARMS", 224);    // Gold amber

    // 4. Digital Number Readouts
    // Ammo Count (e.g. 50 in gold amber)
    draw_hud_number(vram, 22, 181, player_ammo, 224);

    // Health Percentage (e.g. 100% in blood red)
    int hw = draw_hud_number(vram, 80, 181, player_health, (player_health > 30) ? 176 : 179);
    draw_hud_percent(vram, 80 + hw + 2, 181, (player_health > 30) ? 176 : 179);

    // Armor Percentage (100% in bright green)
    int aw = draw_hud_number(vram, 192, 181, 100, 112);
    draw_hud_percent(vram, 192 + aw + 2, 181, 112);

    // Arms Display (Shell count & weapon slot)
    draw_hud_text(vram, 266, 181, "SHEL", 224);
    draw_hud_number(vram, 288, 181, player_ammo, 224);

    // 5. Authentic Doomguy Face Portrait (Center x = 148, y = 170)
    for (int y = 169; y <= 198; y++) {
        vram[y * SCREEN_W + 144] = 108;
        vram[y * SCREEN_W + 176] = 96;
    }
    for (int x = 144; x <= 176; x++) {
        vram[169 * SCREEN_W + x] = 108;
        vram[198 * SCREEN_W + x] = 96;
    }
    const doom_patch_t *face = &patch_stfst00;
    int face_x = (SCREEN_W - face->width) / 2; // (320 - 24) / 2 = 148
    int face_y = 170;
    draw_authentic_patch(vram, face, face_x, face_y);
}

// ==============================================================================
// Render First-Person Super Shotgun (Scaled ~1.2x for Optimal Viewport Visibility)
// ==============================================================================
static void render_weapon(volatile uint8_t *vram) {
    const doom_patch_t *wpn = &patch_sht2a0;
    int orig_w = wpn->width;  // 59
    int orig_h = wpn->height; // 55
    int wpn_w = (orig_w * 6) / 5; // 70 pixels wide (~1.2x scale, authentic compact proportion)
    int wpn_h = (orig_h * 6) / 5; // 66 pixels high
    int wpn_x = (SCREEN_W - wpn_w) / 2; // (320 - 70) / 2 = 125
    int wpn_y = VIEWPORT_H - wpn_h;      // 168 - 66 = 102 (locked to weapon_top_clip)

    draw_authentic_patch_weapon(vram, wpn, wpn_x, wpn_y, wpn_w, wpn_h);

    if (muzzle_flash > 0) {
        int barrel_cx = wpn_x + (wpn_w / 2);
        int barrel_y = wpn_y - 4;
        for (int dy = -10; dy <= 0; dy++) {
            for (int dx = -20; dx <= 20; dx++) {
                int dist_sq = dx * dx + dy * dy * 3;
                if (dist_sq < 300) {
                    uint8_t col = (dist_sq < 60) ? 4 : ((dist_sq < 180) ? 224 : 176);
                    put_pixel(vram, barrel_cx + dx, barrel_y + dy, col);
                }
            }
        }
    }
}

// ==============================================================================
// Render Small Crisp Aiming Crosshair at Viewport Center (160, 84)
// ==============================================================================
static void render_crosshair(volatile uint8_t *vram) {
    int cx = CENTER_X; // 160
    int cy = CENTER_Y; // 84

    uint8_t cross_col = 112; // Bright green (DOOM 2 index 112)
    uint8_t dot_col   = 4;   // White center dot (DOOM 2 index 4)

    // Center dot
    vram[cy * SCREEN_W + cx] = dot_col;

    // 4 cardinal ticks
    for (int d = 2; d <= 4; d++) {
        vram[cy * SCREEN_W + (cx - d)] = cross_col;
        vram[cy * SCREEN_W + (cx + d)] = cross_col;
        vram[(cy - d) * SCREEN_W + cx] = cross_col;
        vram[(cy + d) * SCREEN_W + cx] = cross_col;
    }
}

// ==============================================================================
// Render 2D Minimap Overlay in Top-Left Corner (52x55)
// ==============================================================================
static void render_minimap(volatile uint8_t *vram) {
    if (!show_minimap || !cur_map) return;

    // Draw lines of the level (North is UP)
    for (int i = 0; i < cur_map->num_lines; i++) {
        const line_t *line = &cur_map->lines[i];
        int x1 = (FROM_FIXED(line->v1->x) + 380) / 16 + 4;
        int y1 = 55 - (FROM_FIXED(line->v1->y) + 200) / 16;
        int x2 = (FROM_FIXED(line->v2->x) + 380) / 16 + 4;
        int y2 = 55 - (FROM_FIXED(line->v2->y) + 200) / 16;

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

    // Draw player position and heading indicator (North is UP)
    int px = (FROM_FIXED(viewx) + 380) / 16 + 4;
    int py = 55 - (FROM_FIXED(viewy) + 200) / 16;
    put_pixel(vram, px, py, 4); // White dot
    int dir_x = px + FROM_FIXED(viewcos * 4);
    int dir_y = py - FROM_FIXED(viewsin * 4); // Negative Y on screen is UP (North)
    put_pixel(vram, dir_x, dir_y, 176); // Red tick
}

// ==============================================================================
// 3D Frame Rendering Pipeline: BSP Walk, Linedefs, Mobjs, Weapon & HUD
static inline uint32_t read_mcycle(void) {
    uint32_t c;
    asm volatile ("csrr %0, mcycle" : "=r"(c));
    return c;
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

    // 2. Set Camera Context with Dynamic Floor Elevation Tracking
    if (player_mobj) {
        fixed_t target_z = P_GetSectorFloorAt(player_mobj->x, player_mobj->y);
        if (player_mobj->z < target_z) {
            player_mobj->z += TO_FIXED(4); // Smooth step up
            if (player_mobj->z > target_z) player_mobj->z = target_z;
        } else if (player_mobj->z > target_z) {
            player_mobj->z -= TO_FIXED(8); // Gravity fall down ledges
            if (player_mobj->z < target_z) player_mobj->z = target_z;
        }
        viewx = player_mobj->x;
        viewy = player_mobj->y;
        viewz = player_mobj->z + TO_FIXED(41); // Eye height = 41 units
    }
    viewcos = get_cos(player_angle_idx);
    viewsin = get_sin(player_angle_idx);

    // 3. Render 3D World Geometry Strictly Front-to-Back (Walls, Portals, Flats)
    R_RenderWorld();

    // 4. Render Dynamic 2.5D Mobjs (Imps, Demons, Shotgun Guys) via GEMM or CPU
    R_DrawMasked();
    gemm_batches++;

    // 5. Render Authentic First-Person Super Shotgun
    render_weapon(vram);

    // 6. Render Small Aiming Crosshair
    render_crosshair(vram);

    // 7. Render Authentic Status Bar HUD
    render_status_bar(vram);

    // 8. Render Minimap Overlay
    render_minimap(vram);

    // 9. Real-Time FPS Calculation and Top-Right HUD Display (HW & SW Mode)
    static uint32_t fps_timer = 0;
    static int frame_counter = 0;
    static int display_fps = 30;

    frame_counter++;
    uint32_t now = read_mcycle();
    uint32_t clk = NEORV32_SYSINFO->CLK;
    if (clk == 0) clk = 100000000;
    uint32_t elapsed = now - fps_timer;
    if (elapsed >= (clk / 2)) {
        display_fps = (int)((frame_counter * (clk / 1000)) / (elapsed / 1000));
        frame_counter = 0;
        fps_timer = now;
    }

    int fx = 232;
    fx += draw_hud_number(vram, fx, 4, display_fps, 112); // Green FPS digits
    draw_hud_text(vram, fx + 2, 5, "FPS", 224);           // Amber "FPS" label
    draw_hud_text(vram, fx + 18, 5, use_gemm_hw ? "[HW]" : "[SW]", use_gemm_hw ? 112 : 176);

    frame_count++;
}

// ==============================================================================
// Player Input & Collision Coordinator
// ==============================================================================
static void handle_input(char cmd) {
    fixed_t move_speed = TO_FIXED(12);

    switch (cmd) {
        case 'w': case 'W': { // Forward with Wall Collision & Axial Sliding
            if (player_mobj) {
                fixed_t mx = FIXED_MUL(viewcos, move_speed);
                fixed_t my = FIXED_MUL(viewsin, move_speed);
                fixed_t nx = player_mobj->x + mx;
                fixed_t ny = player_mobj->y + my;
                if (P_CheckPosition(nx, ny)) {
                    player_mobj->x = nx;
                    player_mobj->y = ny;
                } else if (P_CheckPosition(nx, player_mobj->y)) {
                    player_mobj->x = nx; // Slide in X
                } else if (P_CheckPosition(player_mobj->x, ny)) {
                    player_mobj->y = ny; // Slide in Y
                }
            }
            break;
        }
        case 's': case 'S': { // Backward with Wall Collision & Axial Sliding
            if (player_mobj) {
                fixed_t mx = FIXED_MUL(viewcos, move_speed);
                fixed_t my = FIXED_MUL(viewsin, move_speed);
                fixed_t nx = player_mobj->x - mx;
                fixed_t ny = player_mobj->y - my;
                if (P_CheckPosition(nx, ny)) {
                    player_mobj->x = nx;
                    player_mobj->y = ny;
                } else if (P_CheckPosition(nx, player_mobj->y)) {
                    player_mobj->x = nx; // Slide in X
                } else if (P_CheckPosition(player_mobj->x, ny)) {
                    player_mobj->y = ny; // Slide in Y
                }
            }
            break;
        }
        case 'a': case 'A': // Turn Left
            player_angle_idx = (player_angle_idx + 1) & 63;
            break;
        case 'd': case 'D': // Turn Right
            player_angle_idx = (player_angle_idx - 1) & 63;
            break;
        case ' ': case 'f': case 'F': // FIRE SUPER SHOTGUN!
            if (player_ammo > 0) {
                player_ammo -= 2;
                muzzle_flash = 3;
                neorv32_uart0_printf("[DOOM 2] BOOM! Ammo: %d\n", player_ammo);

                // Inflict buckshot damage
                for (int i = 0; i < num_mobjs; i++) {
                    mobj_t *mo = &mobjs[i];
                    if (mo->type != MT_PLAYER && mo->health > 0) {
                        int d_x = FROM_FIXED(mo->x - viewx);
                        int d_y = FROM_FIXED(mo->y - viewy);
                        if (d_x * d_x + d_y * d_y < 500 * 500) {
                            mo->health -= 120;
                            mo->state = (mo->health <= 0) ? S_IMP_DIE1 : S_IMP_PAIN;
                        }
                    }
                }
            }
            break;
        case 'h': case 'H': case 't': case 'T': // Toggle HW / SW Render Mode
            use_gemm_hw = !use_gemm_hw;
            neorv32_uart0_printf("[MODE] %s\n", use_gemm_hw ? "HW (GEMM Spans)" : "SW (CPU Columns)");
            break;
        case '1':
            P_LoadMap(0);
            neorv32_uart0_printf("[MAP] %s\n", cur_map->name);
            break;
        case '2':
            P_LoadMap(1);
            neorv32_uart0_printf("[MAP] %s\n", cur_map->name);
            break;
        case 'n': case 'N':
            P_LoadMap((cur_map_idx + 1) % NUM_MAPS);
            neorv32_uart0_printf("[MAP] %s\n", cur_map->name);
            break;
        case 'm': case 'M': // Toggle Minimap
            show_minimap = !show_minimap;
            break;
        case 'r': case 'R': // Respawn
            P_LoadMap(cur_map_idx);
            neorv32_uart0_printf("[RESET] %s\n", cur_map->name);
            break;
        case 'b': case 'B': case 'u': case 'U': case 'q': case 'Q':
            neorv32_uart0_printf("\n[REBOOT]\n");
            {
                void (*boot_entry)(void) = (void(*)(void))NEORV32_BOOTROM_BASE;
                boot_entry();
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

    neorv32_uart0_printf("\n[DOOM 2] GEMMrv32 320x200 Mode 13h | Dual-Mode Engine\n");

    // 0. Clear 64,000-byte VRAM to black (byte-by-byte for hardware BRAM interface)
    volatile uint8_t *vram_init = (volatile uint8_t *)VGA_VRAM_BASE;
    for (int i = 0; i < VRAM_BYTES; i++) {
        vram_init[i] = 0;
    }

    // 1. Initialize 256-color 12-bit RGB Palette from authentic DOOM 2 PLAYPAL
    setup_authentic_doom2_palette();
    R_InitWeaponClip();

    // 3. Initialize Map Geometry & Mobjs
    P_InitMaps();

    // 4. Render Initial Native 320x200 Frame
    render_doom_v3_frame();
    neorv32_uart0_printf("[DOOM 2] Ready. Level: %s\n", cur_map->name);

    int last_report = 0;

    while (1) {
        // Poll UART0 (PC Keyboard) - Drain entire RX FIFO to eliminate input lag!
        while (neorv32_uart0_char_received()) {
            char cmd = neorv32_uart0_char_received_get();
            handle_input(cmd);
        }

        // Poll PS/2 Keyboard Port - Drain entire FIFO
        while (*((volatile uint32_t *)PS2_STAT_REG) & 1) {
            uint8_t sc = (uint8_t)(*((volatile uint32_t *)PS2_DATA_REG) & 0xFF);
            char ch = ps2_scancode_to_ascii(sc);
            if (ch) {
                handle_input(ch);
            }
        }

        // 35 Hz Ticker: Advance Monster AI, Fireball Projectiles, and Animations
        P_Ticker();

        // Render Frame continuously for fluid, retention-free gameplay
        render_doom_v3_frame();
        if (muzzle_flash > 0) muzzle_flash--;

        if (frame_count - last_report >= 30) {
            last_report = frame_count;
            neorv32_uart0_printf("[DOOM 2] Frame: %u | GEMM Batches: %u | Health: %d | Ammo: %d\n",
                frame_count, gemm_batches, player_health, player_ammo);
        }
    }

    return 0;
}
