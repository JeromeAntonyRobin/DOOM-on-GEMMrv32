#include <neorv32.h>
#include <stdint.h>
#include <stdbool.h>
#include "gemm_lib/gemmrv_gpu.h"
#include "gemm_lib/gemmrv_hw.h"

// ==============================================================================
// Memory-Mapped I/O Base Addresses
// ==============================================================================
#define VGA_VRAM_BASE       0x90000000UL  // 160x120 Framebuffer (19,200 bytes)
#define VGA_PALETTE_BASE    0x90010000UL  // 256 x 12-bit RGB Color Lookup Table
#define VGA_CTRL_BASE       0x90010400UL  // Status Register ([0]: in_vblank)

#define PS2_DATA_REG        0x90020000UL  // Keyboard Scancode FIFO read
#define PS2_STAT_REG        0x90020004UL  // Keyboard Status ([0]: data_ready, [1]: full)

#define SCREEN_W            160
#define SCREEN_H            120
#define TOTAL_PIXELS        (SCREEN_W * SCREEN_H)

#define MAP_W               16
#define MAP_H               16

// ==============================================================================
// Fixed-Point Mathematics (16.16)
// ==============================================================================
#define FP_SHIFT            16
#define FP_ONE              (1 << FP_SHIFT)
#define TO_FP(x)            ((int32_t)((x) * FP_ONE))
#define FROM_FP(x)          ((x) >> FP_SHIFT)
#define FP_MUL(a, b)        ((int32_t)(((int64_t)(a) * (b)) >> FP_SHIFT))
#define FP_DIV(a, b)        ((int32_t)(((int64_t)(a) << FP_SHIFT) / (b)))

// ==============================================================================
// E1M1 Hangar Inspired Map Layout (16x16)
// 0: Empty, 1: Tech Blue, 2: Gray Stone, 3: Red Terminal, 4: Wood/Rust
// ==============================================================================
static const uint8_t worldMap[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1},
    {1,0,2,2,0,0,1,0,3,3,3,0,0,0,0,1},
    {1,0,2,2,0,0,0,0,3,0,3,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,3,3,3,0,0,4,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,0,0,4,0,1},
    {1,1,0,1,1,1,1,0,0,0,0,0,0,4,0,1},
    {1,0,0,0,0,0,1,0,0,2,2,0,0,0,0,1},
    {1,0,0,0,0,0,1,0,0,2,2,0,0,0,0,1},
    {1,0,3,3,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,3,3,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,4,4,4,0,0,1,1,0,0,1},
    {1,0,0,0,0,0,4,0,4,0,0,1,1,0,0,1},
    {1,0,2,0,0,0,4,4,4,0,0,0,0,0,0,1},
    {1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// ==============================================================================
// 3D Dynamic Entities (Accelerated via 8x8 Systolic GEMM Coprocessor)
// ==============================================================================
typedef struct {
    int8_t x;       // World X
    int8_t y;       // World Y
    int8_t z;       // World Z (elevation)
    uint8_t type;   // 1=Barrel, 2=Imp Demon, 3=Health Pack, 4=Armor
} Entity;

static Entity worldEntities[8] = {
    { 3,  3, 0, 1 }, // Explosive Barrel
    { 5,  2, 0, 3 }, // Health Pack
    { 9,  3, 0, 2 }, // Imp Demon
    {13,  4, 0, 1 }, // Explosive Barrel
    { 3, 10, 0, 4 }, // Green Armor
    {10,  8, 0, 2 }, // Imp Demon
    { 7, 13, 0, 1 }, // Explosive Barrel
    {13, 13, 0, 3 }  // Health Pack
};

// Transformed entity coordinates in Camera Space: [X_cam, Y_cam, Z_cam, Dist]
static int32_t entityCamCoords[8 * 4];

// ==============================================================================
// Color Palette Initialization (256 Colors for DOOM Shading)
// ==============================================================================
static void setup_doom_palette(void) {
    // 0: Sky / Ceiling (Dark Midnight Navy Blue)
    vga_set_palette_entry(0, 0x002);
    // 1: Floor (Toxic Dark Brown / Charcoal)
    vga_set_palette_entry(1, 0x210);

    // 2..7: Basic Primaries
    vga_set_palette_entry(2, 0x00F); // Blue
    vga_set_palette_entry(3, 0x0F0); // Green
    vga_set_palette_entry(4, 0x0FF); // Cyan
    vga_set_palette_entry(5, 0xF00); // Red
    vga_set_palette_entry(6, 0xFF0); // Yellow
    vga_set_palette_entry(7, 0xFFF); // White

    // 10..19: Wall Type 1 - Tech Blue Gradient (light to dark for distance)
    for (int i = 0; i < 10; i++) {
        int b = 15 - i;
        if (b < 2) b = 2;
        vga_set_palette_entry(10 + i, (0 << 8) | (b/3 << 4) | b);
    }

    // 20..29: Wall Type 2 - Slime / Stone Gray Gradient
    for (int i = 0; i < 10; i++) {
        int g = 14 - i;
        if (g < 2) g = 2;
        vga_set_palette_entry(20 + i, (g/2 << 8) | (g << 4) | (g/2));
    }

    // 30..39: Wall Type 3 - Red Warning / Computer Gradient
    for (int i = 0; i < 10; i++) {
        int r = 15 - i;
        if (r < 2) r = 2;
        vga_set_palette_entry(30 + i, (r << 8) | (r/4 << 4) | 0);
    }

    // 40..49: Wall Type 4 - Steel / Wood Rust Gradient
    for (int i = 0; i < 10; i++) {
        int v = 13 - i;
        if (v < 2) v = 2;
        vga_set_palette_entry(40 + i, (v << 8) | (v/2 << 4) | (v/4));
    }

    // 50..55: Minimap Colors
    vga_set_palette_entry(50, 0x333); // Minimap background
    vga_set_palette_entry(51, 0x888); // Minimap wall
    vga_set_palette_entry(52, 0x0F0); // Minimap player
    vga_set_palette_entry(53, 0xF00); // Minimap enemy/entity
    vga_set_palette_entry(54, 0xFF0); // Minimap ray frustum
}

// ==============================================================================
// Player State (Spawns in open room looking down long corridor)
// ==============================================================================
static int32_t posX = TO_FP(7.5);
static int32_t posY = TO_FP(9.5);
static int32_t dirX = TO_FP(1.0);
static int32_t dirY = TO_FP(0.0);
static int32_t planeX = TO_FP(0.0);
static int32_t planeY = TO_FP(0.66); // ~66 degree FOV

static bool show_minimap = true;
static uint32_t frame_count = 0;
static uint32_t gemm_transform_count = 0;

// Rotate vectors using fixed-point matrix multiplication
static void rotate_player(int32_t cos_val, int32_t sin_val) {
    int32_t oldDirX = dirX;
    dirX = FP_MUL(dirX, cos_val) - FP_MUL(dirY, sin_val);
    dirY = FP_MUL(oldDirX, sin_val) + FP_MUL(dirY, cos_val);

    int32_t oldPlaneX = planeX;
    planeX = FP_MUL(planeX, cos_val) - FP_MUL(planeY, sin_val);
    planeY = FP_MUL(oldPlaneX, sin_val) + FP_MUL(planeY, cos_val);
}

// ==============================================================================
// GEMM Systolic Array: Transform All 8 World Entities to Camera Space
// ==============================================================================
static void update_entities_with_gemm(void) {
    // Relative coordinates: delta_X = Entity.X - Player.X, delta_Y = Entity.Y - Player.Y
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

    // Pack camera direction / rotation into 4x4 matrix (fixed-point 8-bit scale: 64 = 1.0)
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
    gemm_transform_count++;
}

// ==============================================================================
// 3D Raycasting Core Engine
// ==============================================================================
static void render_doom_frame(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    // Cast 160 rays across the horizontal screen width.
    // Each column x is rasterized in a single continuous vertical pass:
    // [0 .. drawStart-1] = Ceiling, [drawStart .. drawEnd] = Wall, [drawEnd+1 .. 119] = Floor.
    // This guarantees 100% pixel overwrite using byte writes, eliminating residual ghost textures.
    for (int x = 0; x < SCREEN_W; x++) {
        // cameraX in [-FP_ONE, +FP_ONE]
        int32_t cameraX = TO_FP(2 * x) / SCREEN_W - FP_ONE;

        int32_t rayDirX = dirX + FP_MUL(planeX, cameraX);
        int32_t rayDirY = dirY + FP_MUL(planeY, cameraX);

        int mapX = FROM_FP(posX);
        int mapY = FROM_FP(posY);

        // deltaDist = |1 / rayDir|
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

        // Perform DDA traversal
        int hit = 0;
        int side = 0; // 0: X-axis (E/W), 1: Y-axis (N/S)
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

        // Calculate perpendicular wall distance (eliminates fisheye distortion)
        int32_t perpWallDist;
        if (side == 0) {
            int32_t numer = TO_FP(mapX) - posX + TO_FP(1 - stepX) / 2;
            perpWallDist = (rayDirX != 0) ? FP_DIV(numer, rayDirX) : FP_ONE;
        } else {
            int32_t numer = TO_FP(mapY) - posY + TO_FP(1 - stepY) / 2;
            perpWallDist = (rayDirY != 0) ? FP_DIV(numer, rayDirY) : FP_ONE;
        }

        if (perpWallDist <= 0) perpWallDist = TO_FP(0.1);

        // Projected line height on screen
        int lineHeight = FROM_FP(FP_DIV(TO_FP(SCREEN_H), perpWallDist));
        if (lineHeight > SCREEN_H * 2) lineHeight = SCREEN_H * 2;

        int drawStart = -lineHeight / 2 + SCREEN_H / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_H / 2;
        if (drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

        // Distance shading band (0 = closest/brightest, 9 = farthest/darkest)
        int shade = FROM_FP(perpWallDist);
        if (shade > 9) shade = 9;

        // Base color index for wall type
        uint8_t baseColor;
        switch (wallType) {
            case 1:  baseColor = 10; break; // Tech Blue
            case 2:  baseColor = 20; break; // Slime Stone
            case 3:  baseColor = 30; break; // Red Computer
            default: baseColor = 40; break; // Steel / Rust
        }

        uint8_t pixelColor = baseColor + shade;
        // Darken side 1 for 3D depth perception
        if (side == 1 && shade < 9) {
            pixelColor += 1;
        }

        // Single-pass column rasterization: Ceiling -> Wall -> Floor
        int vram_idx = x;
        // 1. Ceiling slice
        for (int y = 0; y < drawStart; y++) {
            vram[vram_idx] = 0; // Dark Blue Ceiling
            vram_idx += SCREEN_W;
        }
        // 2. 3D Wall slice
        for (int y = drawStart; y <= drawEnd; y++) {
            vram[vram_idx] = pixelColor;
            vram_idx += SCREEN_W;
        }
        // 3. Floor slice
        for (int y = drawEnd + 1; y < SCREEN_H; y++) {
            vram[vram_idx] = 1; // Dark Stone Floor
            vram_idx += SCREEN_W;
        }
    }

    // 3. Render 3D Minimap Overlay (Top-Left 32x32 pixels)
    if (show_minimap) {
        int mm_scale = 2; // 2x2 pixels per grid cell (32x32 total)
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

        // Draw Player on Minimap
        int px = FROM_FP(posX) * mm_scale + 2;
        int py = FROM_FP(posY) * mm_scale + 2;
        if (px >= 2 && px < 34 && py >= 2 && py < 34) {
            vram[py * SCREEN_W + px] = 52; // Bright green
        }

        // Draw Camera Direction Ray on Minimap
        int f_px = px + FROM_FP(dirX * 4);
        int f_py = py + FROM_FP(dirY * 4);
        if (f_px >= 2 && f_px < 34 && f_py >= 2 && f_py < 34) {
            vram[f_py * SCREEN_W + f_px] = 54; // Yellow
        }
    }

    frame_count++;
}

// ==============================================================================
// Movement & Collision Handling
// ==============================================================================
static void handle_input(char cmd) {
    int32_t moveSpeed = TO_FP(0.20);
    // Rotation angle ~0.12 radians: cos(0.12) ~ 0.992, sin(0.12) ~ 0.119
    int32_t cos_rot = TO_FP(0.9928);
    int32_t sin_rot = TO_FP(0.1197);

    int32_t newPosX = posX;
    int32_t newPosY = posY;

    switch (cmd) {
        case 'w':
        case 'W':
            newPosX += FP_MUL(dirX, moveSpeed);
            newPosY += FP_MUL(dirY, moveSpeed);
            break;
        case 's':
        case 'S':
            newPosX -= FP_MUL(dirX, moveSpeed);
            newPosY -= FP_MUL(dirY, moveSpeed);
            break;
        case 'a':
        case 'A':
            // Turn Left
            rotate_player(cos_rot, -sin_rot);
            break;
        case 'd':
        case 'D':
            // Turn Right
            rotate_player(cos_rot, sin_rot);
            break;
        case 'm':
        case 'M':
            show_minimap = !show_minimap;
            neorv32_uart0_printf("[DOOM] Minimap: %s\n", show_minimap ? "ON" : "OFF");
            break;
        case 'r':
        case 'R':
            posX = TO_FP(7.5);
            posY = TO_FP(9.5);
            dirX = TO_FP(1.0);
            dirY = TO_FP(0.0);
            planeX = TO_FP(0.0);
            planeY = TO_FP(0.66);
            neorv32_uart0_printf("[DOOM] Player position reset to spawn point (7.5, 9.5)\n");
            break;
        default:
            return;
    }

    // Collision detection: ensure target grid cell is empty (0)
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
    neorv32_uart0_printf("   GEMMrv32 DOOM 3D Workstation Active               \n");
    neorv32_uart0_printf("   VGA Resolution: 160x120 -> 640x480 @ 60Hz 12-bit  \n");
    neorv32_uart0_printf("   GPU Coprocessor: 8x8 Systolic GEMM Array Enabled  \n");
    neorv32_uart0_printf("   Controls: W=Forward, S=Back, A=Left, D=Right       \n");
    neorv32_uart0_printf("             M=Toggle Minimap, R=Respawn             \n");
    neorv32_uart0_printf("======================================================\n\n");

    setup_doom_palette();

    // Initial world entity transformation using GEMM accelerator
    update_entities_with_gemm();
    neorv32_uart0_printf("[GEMM GPU] 8x8 Systolic Array initialized! Entity batch transformed.\n");

    // Render initial 3D frame
    render_doom_frame();
    neorv32_uart0_printf("[DOOM 3D] First frame rendered successfully! Waiting for input...\n");

    int last_report = 0;

    while (1) {
        bool state_changed = false;

        // 1. Poll UART0 (PC Keyboard via Serial Terminal)
        if (neorv32_uart0_available()) {
            char cmd = (char)neorv32_uart0_getc();
            handle_input(cmd);
            state_changed = true;
        }

        // 2. Poll Onboard PS/2 Keyboard Controller (Basys 3 Port J2)
        if (*((volatile uint32_t *)PS2_STAT_REG) & 1) {
            uint8_t sc = (uint8_t)(*((volatile uint32_t *)PS2_DATA_REG) & 0xFF);
            char ch = ps2_scancode_to_ascii(sc);
            if (ch) {
                handle_input(ch);
                state_changed = true;
            }
        }

        // 3. Re-render frame whenever player moves or rotates
        if (state_changed) {
            // Update entity coordinates via GEMM coprocessor
            update_entities_with_gemm();
            // Draw new 3D view
            render_doom_frame();

            if (frame_count - last_report >= 10) {
                last_report = frame_count;
                neorv32_uart0_printf("[DOOM] Pos: (%d.%02d, %d.%02d) | Frames: %u | GEMM Batches: %u\n",
                    FROM_FP(posX), (int)((posX & 0xFFFF) * 100 / FP_ONE),
                    FROM_FP(posY), (int)((posY & 0xFFFF) * 100 / FP_ONE),
                    frame_count, gemm_transform_count);
            }
        }
    }

    return 0;
}
