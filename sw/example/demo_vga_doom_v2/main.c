// ==============================================================================
// GEMMrv32 DOOM v2: Enhanced 3D Engine with Textures, Sprites & HUD
// Hardware Target: Digilent Basys 3 FPGA (Artix-7 XC7A35T)
// CPU: NEORV32 RV32IM @ 50 MHz | GPU: 8x8 Systolic GEMM Accelerator
// Video: 640x480 @ 60 Hz 12-bit RGB (160x120 Internal Framebuffer)
// ==============================================================================

#include <stdint.h>
#include <stdbool.h>
#include <neorv32.h>
#include "gemm_lib/gemmrv_gpu.h"
#include "doom_assets.h"

#define SCREEN_W           160
#define SCREEN_H           120
#define VIEWPORT_H         104 // 3D world takes top 104 scanlines (0..103)
#define STATUS_BAR_H        16 // Bottom 16 scanlines (104..119)

#define MAP_W               16
#define MAP_H               16

#define FP_SHIFT            16
#define FP_ONE              (1 << FP_SHIFT)
#define TO_FP(x)            ((int32_t)((x) * FP_ONE))
#define FROM_FP(x)          ((int32_t)((x) >> FP_SHIFT))
#define FP_MUL(a, b)        ((int32_t)(((int64_t)(a) * (int64_t)(b)) >> FP_SHIFT))
#define FP_DIV(a, b)        ((int32_t)((((int64_t)(a)) << FP_SHIFT) / (b)))

// 16x16 Level Map:
// 0: Empty Corridor, 1: Tech Wall, 2: Slime Stone, 3: Computer Console, 4: Steel Plating
static const uint8_t worldMap[MAP_H][MAP_W] = {
    {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4},
    {4,0,0,0,0,0,1,0,0,0,0,0,0,0,0,4},
    {4,0,1,1,0,0,1,0,3,3,3,3,0,2,0,4},
    {4,0,1,0,0,0,0,0,3,0,0,3,0,2,0,4},
    {4,0,1,0,0,0,1,0,3,0,0,3,0,0,0,4},
    {4,0,0,0,0,0,1,0,0,0,0,0,0,2,0,4},
    {4,0,2,2,2,0,1,1,1,0,1,1,1,2,0,4},
    {4,0,2,0,2,0,0,0,0,0,0,0,0,0,0,4},
    {4,0,2,0,2,0,1,0,0,0,0,1,0,3,0,4},
    {4,0,0,0,0,0,1,0,0,0,0,1,0,3,0,4},
    {4,3,3,0,3,3,1,0,0,0,0,1,0,3,0,4},
    {4,0,0,0,0,0,0,0,2,2,0,0,0,0,0,4},
    {4,0,1,1,1,0,0,0,2,2,0,0,1,1,0,4},
    {4,0,1,0,0,0,3,0,0,0,0,0,1,0,0,4},
    {4,0,0,0,0,0,3,0,0,0,0,0,0,0,0,4},
    {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4}
};

// Player State (Spawn in open hallway at 7.5, 9.5 facing North/East)
static int32_t posX   = TO_FP(7.5);
static int32_t posY   = TO_FP(9.5);
static int32_t dirX   = TO_FP(1.0);
static int32_t dirY   = TO_FP(0.0);
static int32_t planeX = TO_FP(0.0);
static int32_t planeY = TO_FP(0.66); // 66 degree FOV

// Game Stats
static int player_health = 100;
static int player_ammo   = 50;
static int player_score  = 0;

static uint32_t frame_count = 0;
static uint32_t gemm_batches = 0;
static bool show_minimap = true;
static int muzzle_flash = 0; // Countdown frames for weapon flash
static int walk_bob = 0;

// 1D Depth Buffer for Sprite Occlusion
static int32_t ZBuffer[SCREEN_W];

// 8 Dynamic World Entities (Monsters, Barrels, Pickups)
typedef struct {
    int x, y, z;
    int type; // 0: Imp, 1: Barrel, 2: Medkit
} WorldEntity;

static WorldEntity worldEntities[8] = {
    {9,  9, 0, 0}, // Imp Demon 1
    {11, 9, 0, 0}, // Imp Demon 2
    {10, 8, 0, 1}, // Explosive Barrel 1
    {8,  7, 0, 1}, // Explosive Barrel 2
    {12, 7, 0, 1}, // Explosive Barrel 3
    {9,  4, 0, 2}, // Health Medkit 1
    {13, 3, 0, 0}, // Imp Demon 3
    {5,  9, 0, 2}  // Health Medkit 2
};

static int32_t entityCamCoords[8 * 4] __attribute__((aligned(32)));

// ==============================================================================
// 12-Bit RGB Palette Configuration (256 Colors)
// ==============================================================================
static void setup_enhanced_palette(void) {
    volatile uint32_t *clut = (volatile uint32_t *)VGA_PALETTE_BASE;

    // 0: Dark Blue Ceiling (RGB 1, 1, 3)
    clut[0] = 0x113;
    // 1: Dark Stone Floor (RGB 2, 2, 2)
    clut[1] = 0x222;

    // 10..19: Tech Blue Wall Shades (10 closest/brightest -> 19 farthest/darkest)
    for (int i = 0; i < 10; i++) {
        int r = (9 - i) / 5;
        int g = (9 - i) / 3 + 1;
        int b = 15 - (i * 12 / 10);
        clut[10 + i] = ((r & 0xF) << 8) | ((g & 0xF) << 4) | (b & 0xF);
    }

    // 20..29: Slime Green Wall Shades
    for (int i = 0; i < 10; i++) {
        int r = (9 - i) / 4;
        int g = 14 - (i * 11 / 10);
        int b = (9 - i) / 4;
        clut[20 + i] = ((r & 0xF) << 8) | ((g & 0xF) << 4) | (b & 0xF);
    }

    // 30..39: Red Computer Wall Shades
    for (int i = 0; i < 10; i++) {
        int r = 15 - (i * 12 / 10);
        int g = (9 - i) / 4;
        int b = (9 - i) / 4;
        clut[30 + i] = ((r & 0xF) << 8) | ((g & 0xF) << 4) | (b & 0xF);
    }

    // 40..49: Steel / Rust Wall Shades
    for (int i = 0; i < 10; i++) {
        int r = 12 - (i * 9 / 10);
        int g = 9 - (i * 7 / 10);
        int b = 6 - (i * 5 / 10);
        clut[40 + i] = ((r & 0xF) << 8) | ((g & 0xF) << 4) | (b & 0xF);
    }

    // 50..59: UI and Minimap Colors
    clut[50] = 0x050; // Dark green radar
    clut[51] = 0x0E0; // Bright green radar wall
    clut[52] = 0xFFF; // Pure White
    clut[53] = 0xFA0; // Orange Amber
    clut[54] = 0xF00; // Bright Red
    clut[55] = 0x0FF; // Cyan

    // 60..69: Imp demon brown/tan flesh shades
    clut[60] = 0x421; clut[61] = 0x632; clut[62] = 0x853;
    clut[63] = 0xA74; clut[64] = 0x310; clut[65] = 0x210;

    // 70..79: Toxic Barrel Acid Green
    clut[70] = 0x0A0; clut[71] = 0x2E1; clut[72] = 0x1B0;
    clut[73] = 0x080; clut[74] = 0x060; clut[75] = 0x040; clut[76] = 0x020;

    // 80..89: Gunmetal & Metallic Grey
    for (int i = 0; i < 10; i++) {
        int v = 14 - (i * 12 / 10);
        clut[80 + i] = (v << 8) | (v << 4) | v;
    }

    // 90..99: Muzzle Fire & Explosions (Yellow to Red)
    clut[90] = 0xFF0; // Pure Yellow
    clut[91] = 0xF00; // Pure Red
    clut[92] = 0xF80; // Orange
    clut[93] = 0xFC0; // Gold Amber

    // 100..109: Doomguy Face tones
    clut[100] = 0xFCB; // Bright skin
    clut[101] = 0xDA9; // Medium skin
    clut[102] = 0xB87; // Darker skin shadow
    clut[103] = 0x854; // Deep skin shadow
    clut[104] = 0x411; // Mouth/lips

    // 110..119: Status Bar Industrial Grey Bevel
    clut[110] = 0x555; // Status bar grey
    clut[111] = 0x777; // Highlight bevel
    clut[112] = 0x333; // Shadow bevel
    clut[113] = 0x222; // Deep groove
}

// ==============================================================================
// 8x8 Systolic GEMM Coprocessor Transformation
// ==============================================================================
static void update_entities_with_gemm(void) {
    int8_t in_coords[8 * 4] __attribute__((aligned(32))) = {0};
    int8_t view_matrix[4 * 4] __attribute__((aligned(32))) = {0};

    int player_cell_x = FROM_FP(posX);
    int player_cell_y = FROM_FP(posY);

    for (int i = 0; i < 8; i++) {
        in_coords[i * 4 + 0] = (int8_t)(worldEntities[i].x - player_cell_x);
        in_coords[i * 4 + 1] = (int8_t)(worldEntities[i].y - player_cell_y);
        in_coords[i * 4 + 2] = worldEntities[i].z;
        in_coords[i * 4 + 3] = 1;
    }

    // Camera orientation in fixed-point 8-bit scale: 64 = 1.0
    int8_t c_dirX = (int8_t)(FROM_FP(dirX * 64));
    int8_t c_dirY = (int8_t)(FROM_FP(dirY * 64));

    view_matrix[0 * 4 + 0] = c_dirX;
    view_matrix[0 * 4 + 1] = -c_dirY;
    view_matrix[1 * 4 + 0] = c_dirY;
    view_matrix[1 * 4 + 1] = c_dirX;
    view_matrix[2 * 4 + 2] = 64;
    view_matrix[3 * 4 + 3] = 64;

    // Dispatch to the 8x8 Systolic GEMM Hardware Coprocessor
    gpu_transform_vertices_8x(in_coords, view_matrix, entityCamCoords);
    gemm_batches++;
}

// ==============================================================================
// Render Status Bar HUD (Bottom 16 scanlines: y = 104..119)
// ==============================================================================
static void render_status_bar(volatile uint8_t *vram) {
    // 1. Draw Status Bar Beveled Baseplate
    for (int y = VIEWPORT_H; y < SCREEN_H; y++) {
        uint8_t line_color = 110; // Medium grey
        if (y == VIEWPORT_H)     line_color = 112; // Top border shadow
        else if (y == VIEWPORT_H + 1) line_color = 111; // Bevel highlight
        else if (y == SCREEN_H - 1)   line_color = 113; // Bottom edge

        int row_idx = y * SCREEN_W;
        for (int x = 0; x < SCREEN_W; x++) {
            vram[row_idx + x] = line_color;
        }
    }

    // 2. Draw Doomguy Face in Center (x = 72..87, y = 106..117)
    uint8_t *face_src = (muzzle_flash > 0) ? doomguy_grin : doomguy_face;
    int face_x = (SCREEN_W - FACE_W) / 2;
    int face_y = VIEWPORT_H + 2;

    for (int fy = 0; fy < FACE_H; fy++) {
        int v_idx = (face_y + fy) * SCREEN_W + face_x;
        int s_idx = fy * FACE_W;
        for (int fx = 0; fx < FACE_W; fx++) {
            vram[v_idx + fx] = face_src[s_idx + fx];
        }
    }

    // 3. Status Bar Text Indicators (AMMO on left, HEALTH on right)
    // Draw Ammo Bar (x = 8..40, y = 110..114)
    int ammo_fill = (player_ammo * 32) / 50;
    for (int y = VIEWPORT_H + 6; y <= VIEWPORT_H + 9; y++) {
        int idx = y * SCREEN_W;
        for (int x = 12; x <= 44; x++) {
            vram[idx + x] = (x - 12 <= ammo_fill) ? 92 : 113; // Gold ammo bar
        }
    }

    // Draw Health Bar (x = 116..148, y = 110..114)
    int health_fill = (player_health * 32) / 100;
    for (int y = VIEWPORT_H + 6; y <= VIEWPORT_H + 9; y++) {
        int idx = y * SCREEN_W;
        for (int x = 116; x <= 148; x++) {
            vram[idx + x] = (x - 116 <= health_fill) ? 54 : 113; // Red health bar
        }
    }
}

// ==============================================================================
// Render First-Person Weapon Sprite
// ==============================================================================
static void render_weapon(volatile uint8_t *vram) {
    uint8_t *weapon_src = (muzzle_flash > 0) ? weapon_flash : weapon_shotgun;
    int weapon_x = (SCREEN_W - WEAPON_W) / 2;
    int weapon_y = VIEWPORT_H - WEAPON_H + (walk_bob ? 2 : 0);

    for (int wy = 0; wy < WEAPON_H; wy++) {
        int py = weapon_y + wy;
        if (py < 0 || py >= VIEWPORT_H) continue;
        int v_idx = py * SCREEN_W + weapon_x;
        int s_idx = wy * WEAPON_W;

        for (int wx = 0; wx < WEAPON_W; wx++) {
            uint8_t col = weapon_src[s_idx + wx];
            if (col != COLOR_TRANSPARENT) {
                vram[v_idx + wx] = col;
            }
        }
    }
}

// ==============================================================================
// Render 2.5D Billboarded Sprites (Imps, Barrels, Medkits)
// ==============================================================================
static void render_sprites(volatile uint8_t *vram) {
    // Distance sort order table
    int sprite_order[8];
    for (int i = 0; i < 8; i++) sprite_order[i] = i;

    // Simple bubble sort by camera distance Y (farthest first)
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7 - i; j++) {
            int d1 = entityCamCoords[sprite_order[j] * 4 + 1];
            int d2 = entityCamCoords[sprite_order[j + 1] * 4 + 1];
            if (d1 < d2) {
                int temp = sprite_order[j];
                sprite_order[j] = sprite_order[j + 1];
                sprite_order[j + 1] = temp;
            }
        }
    }

    // Render sorted entities
    for (int i = 0; i < 8; i++) {
        int idx = sprite_order[i];
        int transX = entityCamCoords[idx * 4 + 0];
        int transY = entityCamCoords[idx * 4 + 1];

        // Only draw if object is in front of camera
        if (transY <= 1) continue;

        // Project to screen coordinate
        int spriteScreenX = (SCREEN_W / 2) + (transX * 100) / transY;
        int spriteHeight  = (VIEWPORT_H * 16) / transY;
        if (spriteHeight > VIEWPORT_H) spriteHeight = VIEWPORT_H;
        int spriteWidth   = spriteHeight; // Square aspect

        int drawStartY = -spriteHeight / 2 + VIEWPORT_H / 2;
        if (drawStartY < 0) drawStartY = 0;
        int drawEndY = spriteHeight / 2 + VIEWPORT_H / 2;
        if (drawEndY >= VIEWPORT_H) drawEndY = VIEWPORT_H - 1;

        int drawStartX = -spriteWidth / 2 + spriteScreenX;
        if (drawStartX < 0) drawStartX = 0;
        int drawEndX = spriteWidth / 2 + spriteScreenX;
        if (drawEndX >= SCREEN_W) drawEndX = SCREEN_W - 1;

        uint8_t *sprite_pixels = sprites[worldEntities[idx].type];

        // Draw vertical stripes
        for (int stripe = drawStartX; stripe <= drawEndX; stripe++) {
            // Depth test against Z-Buffer: if occluded by wall, skip stripe
            if (stripe >= 0 && stripe < SCREEN_W) {
                int32_t obj_depth = TO_FP(transY) / 64;
                if (obj_depth >= ZBuffer[stripe]) {
                    continue; // Wall is closer than this sprite stripe!
                }

                int texX = ((stripe - (-spriteWidth / 2 + spriteScreenX)) * SPRITE_W) / spriteWidth;
                if (texX < 0) texX = 0;
                if (texX >= SPRITE_W) texX = SPRITE_W - 1;

                int v_idx = drawStartY * SCREEN_W + stripe;
                for (int y = drawStartY; y <= drawEndY; y++) {
                    int d = y * 256 - VIEWPORT_H * 128 + spriteHeight * 128;
                    int texY = ((d * SPRITE_H) / spriteHeight) / 256;
                    if (texY < 0) texY = 0;
                    if (texY >= SPRITE_H) texY = SPRITE_H - 1;

                    uint8_t col = sprite_pixels[texY * SPRITE_W + texX];
                    if (col != COLOR_TRANSPARENT) {
                        vram[v_idx] = col;
                    }
                    v_idx += SCREEN_W;
                }
            }
        }
    }
}

// ==============================================================================
// 3D Raycasting Core Engine with Textured Walls & Single-Pass Rasterization
// ==============================================================================
static void render_doom_frame(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    // 1. Cast 160 rays across the horizontal screen width
    for (int x = 0; x < SCREEN_W; x++) {
        int32_t cameraX = TO_FP(2 * x) / SCREEN_W - FP_ONE;

        int32_t rayDirX = dirX + FP_MUL(planeX, cameraX);
        int32_t rayDirY = dirY + FP_MUL(planeY, cameraX);

        int mapX = FROM_FP(posX);
        int mapY = FROM_FP(posY);

        int32_t deltaDistX = (rayDirX == 0) ? 0x7FFFFFFF : (rayDirX > 0 ? FP_DIV(FP_ONE, rayDirX) : FP_DIV(FP_ONE, -rayDirX));
        int32_t deltaDistY = (rayDirY == 0) ? 0x7FFFFFFF : (rayDirY > 0 ? FP_DIV(FP_ONE, rayDirY) : FP_DIV(FP_ONE, -rayDirY));

        int stepX, stepY;
        int32_t sideDistX, sideDistY;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = FP_MUL(posX - TO_FP(mapX), deltaDistX);
        } else {
            stepX = 1;
            sideDistX = FP_MUL(TO_FP(mapX + 1) - posX, deltaDistX);
        }

        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = FP_MUL(posY - TO_FP(mapY), deltaDistY);
        } else {
            stepY = 1;
            sideDistY = FP_MUL(TO_FP(mapY + 1) - posY, deltaDistY);
        }

        // DDA traversal
        int hit = 0;
        int side = 0;
        int wallType = 1;

        while (hit == 0) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (mapX >= 0 && mapX < MAP_W && mapY >= 0 && mapY < MAP_H) {
                if (worldMap[mapY][mapX] > 0) {
                    hit = 1;
                    wallType = worldMap[mapY][mapX];
                }
            } else {
                hit = 1;
                wallType = 1;
            }
        }

        // Calculate perpendicular wall distance (eliminates fisheye)
        int32_t perpWallDist;
        if (side == 0) {
            int32_t numer = TO_FP(mapX) - posX + TO_FP(1 - stepX) / 2;
            perpWallDist = (rayDirX != 0) ? FP_DIV(numer, rayDirX) : FP_ONE;
        } else {
            int32_t numer = TO_FP(mapY) - posY + TO_FP(1 - stepY) / 2;
            perpWallDist = (rayDirY != 0) ? FP_DIV(numer, rayDirY) : FP_ONE;
        }

        if (perpWallDist <= 0) perpWallDist = TO_FP(0.1);

        // Store in 1D Z-Buffer for sprite occlusion
        ZBuffer[x] = perpWallDist;

        // Projected line height on viewport
        int lineHeight = FROM_FP(FP_DIV(TO_FP(VIEWPORT_H), perpWallDist));
        if (lineHeight > VIEWPORT_H * 2) lineHeight = VIEWPORT_H * 2;

        int drawStart = -lineHeight / 2 + VIEWPORT_H / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + VIEWPORT_H / 2;
        if (drawEnd >= VIEWPORT_H) drawEnd = VIEWPORT_H - 1;

        // Texture X calculation
        int32_t wallX;
        if (side == 0) {
            wallX = posY + FP_MUL(perpWallDist, rayDirY);
        } else {
            wallX = posX + FP_MUL(perpWallDist, rayDirX);
        }
        wallX &= 0xFFFF; // Fractional part in 16.16
        int texX = (wallX * TEX_WIDTH) >> FP_SHIFT;
        if (side == 0 && rayDirX > 0) texX = TEX_WIDTH - texX - 1;
        if (side == 1 && rayDirY < 0) texX = TEX_WIDTH - texX - 1;

        // Distance lighting shade
        int shade = FROM_FP(perpWallDist) / 2;
        if (shade > 4) shade = 4;

        int texIdx = (wallType >= 1 && wallType <= NUM_TEXTURES) ? (wallType - 1) : 0;
        uint8_t *tex = textures[texIdx];

        // Single-pass column rasterization: Ceiling -> Textured Wall -> Floor
        int vram_idx = x;
        // 1. Ceiling slice
        for (int y = 0; y < drawStart; y++) {
            vram[vram_idx] = 0; // Dark Blue Ceiling
            vram_idx += SCREEN_W;
        }

        // 2. Textured Wall slice
        for (int y = drawStart; y <= drawEnd; y++) {
            int d = y * 256 - VIEWPORT_H * 128 + lineHeight * 128;
            int texY = ((d * TEX_HEIGHT) / lineHeight) / 256;
            if (texY < 0) texY = 0;
            if (texY >= TEX_HEIGHT) texY = TEX_HEIGHT - 1;

            uint8_t pixel = tex[texY * TEX_WIDTH + texX];
            // Darken side 1 for depth contrast
            if (side == 1) pixel += 1;
            vram[vram_idx] = pixel;
            vram_idx += SCREEN_W;
        }

        // 3. Floor slice
        for (int y = drawEnd + 1; y < VIEWPORT_H; y++) {
            vram[vram_idx] = 1; // Dark Stone Floor
            vram_idx += SCREEN_W;
        }
    }

    // 2. Render 2.5D Dynamic Sprites (Imps, Barrels, Medkits)
    render_sprites(vram);

    // 3. Render First-Person Weapon
    render_weapon(vram);

    // 4. Render Status Bar HUD (Bottom 16 scanlines)
    render_status_bar(vram);

    // 5. Render 3D Minimap Radar Overlay (Top-Left 32x32 pixels)
    if (show_minimap) {
        int mm_scale = 2;
        for (int my = 0; my < MAP_H; my++) {
            for (int mx = 0; mx < MAP_W; mx++) {
                uint8_t mm_col = (worldMap[my][mx] > 0) ? 51 : 50;
                for (int dy = 0; dy < mm_scale; dy++) {
                    int py = my * mm_scale + dy + 2;
                    int px = mx * mm_scale + 2;
                    vram[py * SCREEN_W + px] = mm_col;
                    vram[py * SCREEN_W + px + 1] = mm_col;
                }
            }
        }

        // Player marker
        int p_screen_x = FROM_FP(posX) * mm_scale + 2;
        int p_screen_y = FROM_FP(posY) * mm_scale + 2;
        vram[p_screen_y * SCREEN_W + p_screen_x] = 52; // White dot
        // Direction vector tick
        int dir_px = p_screen_x + FROM_FP(dirX * 3);
        int dir_py = p_screen_y + FROM_FP(dirY * 3);
        if (dir_px >= 0 && dir_px < SCREEN_W && dir_py >= 0 && dir_py < SCREEN_H) {
            vram[dir_py * SCREEN_W + dir_px] = 54; // Red tick
        }
    }

    frame_count++;
}

// ==============================================================================
// Movement & Rotation Controls
// ==============================================================================
static void rotate_player(int32_t cos_val, int32_t sin_val) {
    int32_t oldDirX = dirX;
    dirX = FP_MUL(dirX, cos_val) - FP_MUL(dirY, sin_val);
    dirY = FP_MUL(oldDirX, sin_val) + FP_MUL(dirY, cos_val);

    int32_t oldPlaneX = planeX;
    planeX = FP_MUL(planeX, cos_val) - FP_MUL(planeY, sin_val);
    planeY = FP_MUL(oldPlaneX, sin_val) + FP_MUL(planeY, cos_val);
}

static void handle_input(char cmd) {
    int32_t moveSpeed = TO_FP(0.25);
    int32_t rotCos = 65057; // cos(0.12 rad = ~6.8 deg) * 65536
    int32_t rotSin = 7844;  // sin(0.12 rad) * 65536

    int32_t newPosX = posX;
    int32_t newPosY = posY;

    switch (cmd) {
        case 'w': case 'W': // Forward
            newPosX += FP_MUL(dirX, moveSpeed);
            newPosY += FP_MUL(dirY, moveSpeed);
            walk_bob = !walk_bob;
            break;
        case 's': case 'S': // Backward
            newPosX -= FP_MUL(dirX, moveSpeed);
            newPosY -= FP_MUL(dirY, moveSpeed);
            walk_bob = !walk_bob;
            break;
        case 'a': case 'A': // Turn Left
            rotate_player(rotCos, -rotSin);
            break;
        case 'd': case 'D': // Turn Right
            rotate_player(rotCos, rotSin);
            break;
        case ' ': case 'f': case 'F': // FIRE WEAPON!
            if (player_ammo > 0) {
                player_ammo -= 2;
                muzzle_flash = 2; // Trigger 2 frames of flash
                player_score += 10;
                neorv32_uart0_printf("[DOOM] BANG! Shotgun fired! Ammo: %d | Score: %d\n", player_ammo, player_score);
            }
            break;
        case 'm': case 'M': // Toggle Minimap
            show_minimap = !show_minimap;
            break;
        case 'r': case 'R': // Reset / Respawn
            posX   = TO_FP(7.5);
            posY   = TO_FP(9.5);
            dirX   = TO_FP(1.0);
            dirY   = TO_FP(0.0);
            planeX = TO_FP(0.0);
            planeY = TO_FP(0.66);
            player_health = 100;
            player_ammo   = 50;
            neorv32_uart0_printf("[DOOM] Player respawned at (7.5, 9.5)\n");
            break;
        default:
            return;
    }

    // Wall collision detection
    int checkX = FROM_FP(newPosX);
    int checkY = FROM_FP(newPosY);
    if (checkX >= 0 && checkX < MAP_W && checkY >= 0 && checkY < MAP_H) {
        if (worldMap[checkY][checkX] == 0) {
            posX = newPosX;
            posY = newPosY;
        }
    }
}


// ==============================================================================
// Main Entry Point
// ==============================================================================
int main(void) {
    neorv32_uart0_setup(19200, 0);

    neorv32_uart0_printf("\n======================================================\n");
    neorv32_uart0_printf("   GEMMrv32 DOOM v2: Enhanced 3D Workstation Active  \n");
    neorv32_uart0_printf("   VGA Resolution: 160x120 -> 640x480 @ 60Hz 12-bit  \n");
    neorv32_uart0_printf("   Features: 32x32 Wall Textures, 2.5D GEMM Sprites   \n");
    neorv32_uart0_printf("             Shotgun Weapon HUD & Doomguy Status Bar  \n");
    neorv32_uart0_printf("   Controls: W=Forward, S=Back, A=Left, D=Right       \n");
    neorv32_uart0_printf("             SPACE=Fire Weapon, M=Minimap, R=Respawn  \n");
    neorv32_uart0_printf("======================================================\n\n");

    // Initialize 256-color palette
    setup_enhanced_palette();

    // Initialize procedural textures, sprites, weapon, and Doomguy face
    init_doom_assets();
    neorv32_uart0_printf("[ASSETS] Textures, Sprites & HUD initialized in SRAM!\n");

    // Initialize 8x8 Systolic GEMM Array
    update_entities_with_gemm();
    neorv32_uart0_printf("[GEMM GPU] 8x8 Systolic Array active! Entity batch transformed.\n");

    // Render initial 3D frame
    render_doom_frame();
    neorv32_uart0_printf("[DOOM 3D] First textured frame rendered! Waiting for input...\n");

    int last_report = 0;

    while (1) {
        bool state_changed = false;

        // 1. Poll UART0 (PC Keyboard via Serial Terminal)
        if (neorv32_uart0_available()) {
            char cmd = (char)neorv32_uart0_getc();
            handle_input(cmd);
            state_changed = true;
        }

        // 2. Poll Onboard PS/2 Keyboard Controller
        if (*((volatile uint32_t *)PS2_STAT_REG) & 1) {
            uint8_t sc = (uint8_t)(*((volatile uint32_t *)PS2_DATA_REG) & 0xFF);
            char ch = ps2_scancode_to_ascii(sc);
            if (ch) {
                handle_input(ch);
                state_changed = true;
            }
        }

        // 3. Render next frame
        if (state_changed || muzzle_flash > 0) {
            if (muzzle_flash > 0) muzzle_flash--;

            update_entities_with_gemm();
            render_doom_frame();

            if (frame_count - last_report >= 10) {
                last_report = frame_count;
                neorv32_uart0_printf("[DOOM] Pos: (%d.%02d, %d.%02d) | Frames: %u | GEMM Batches: %u | Ammo: %d\n",
                    FROM_FP(posX), (int)((posX & 0xFFFF) * 100 / FP_ONE),
                    FROM_FP(posY), (int)((posY & 0xFFFF) * 100 / FP_ONE),
                    frame_count, gemm_batches, player_ammo);
            }
        }
    }

    return 0;
}
