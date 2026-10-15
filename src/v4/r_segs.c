#include "r_segs.h"
#include "r_bsp.h"
#include "doom_assets_authentic.h"
#include <string.h>

int16_t ceilingclip[SCREEN_W];
int16_t floorclip[SCREEN_W];
fixed_t wall_dist_buffer[SCREEN_W];

#define VIEWPORT_H 168 // 320x168 3D Viewport, bottom 32 lines for Status Bar HUD
#define CENTER_X   (SCREEN_W / 2)
#define CENTER_Y   (VIEWPORT_H / 2)
#define FOCAL_LEN  160

void R_InitSegs(void) {
    R_ClearDrawSegs();
}

void R_ClearDrawSegs(void) {
    for (int x = 0; x < SCREEN_W; x++) {
        ceilingclip[x] = -1;
        floorclip[x]   = VIEWPORT_H;
        wall_dist_buffer[x] = 0x7FFFFFFF;
    }
}

// ------------------------------------------------------------------------------
// Add Linedef: Transform Endpoints to Camera Space & Clip against Near Plane
// ------------------------------------------------------------------------------
void R_AddLine(line_t *line) {
    // Relative coordinates
    fixed_t p1x = line->v1->x - viewx;
    fixed_t p1y = line->v1->y - viewy;
    fixed_t p2x = line->v2->x - viewx;
    fixed_t p2y = line->v2->y - viewy;

    // Rotate into camera space using view angle cos/sin
    fixed_t tz1 = FIXED_MUL(p1x, viewcos) + FIXED_MUL(p1y, viewsin);
    fixed_t tx1 = FIXED_MUL(p1x, viewsin) - FIXED_MUL(p1y, viewcos);

    fixed_t tz2 = FIXED_MUL(p2x, viewcos) + FIXED_MUL(p2y, viewsin);
    fixed_t tx2 = FIXED_MUL(p2x, viewsin) - FIXED_MUL(p2y, viewcos);

    // Near plane clipping (tz > 4)
    fixed_t near_clip = TO_FIXED(4);
    if (tz1 < near_clip && tz2 < near_clip) return; // Entirely behind camera

    // Clip line against near plane
    if (tz1 < near_clip) {
        fixed_t d = tz2 - tz1;
        if (d == 0) return;
        fixed_t frac = FIXED_DIV(near_clip - tz1, d);
        tx1 += FIXED_MUL(tx2 - tx1, frac);
        tz1 = near_clip;
    } else if (tz2 < near_clip) {
        fixed_t d = tz1 - tz2;
        if (d == 0) return;
        fixed_t frac = FIXED_DIV(near_clip - tz2, d);
        tx2 += FIXED_MUL(tx1 - tx2, frac);
        tz2 = near_clip;
    }

    // Perspective projection onto screen X (using 64-bit intermediate to prevent overflow)
    int x1 = CENTER_X + (int)(((int64_t)tx1 * FOCAL_LEN) / tz1);
    int x2 = CENTER_X + (int)(((int64_t)tx2 * FOCAL_LEN) / tz2);

    // Must be front-facing (x1 < x2)
    if (x1 >= x2) return;
    if (x2 < 0 || x1 >= SCREEN_W) return; // Off-screen

    R_StoreWallRange(x1, x2, line, tz1, tz2);
}

// ------------------------------------------------------------------------------
// Store Wall Range: Rasterize Multi-Height Wall Columns & Portals
// ------------------------------------------------------------------------------
void R_StoreWallRange(int x1, int x2, line_t *line, fixed_t tz1, fixed_t tz2) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    int start_x = (x1 < 0) ? 0 : x1;
    int end_x   = (x2 >= SCREEN_W) ? (SCREEN_W - 1) : x2;
    int span_w  = x2 - x1;
    if (span_w <= 0) return;

    sector_t *front = line->frontsector;
    sector_t *back  = line->backsector;
    bool is_twosided = (line->flags & ML_TWOSIDED) && back;

    for (int x = start_x; x <= end_x; x++) {
        if (ceilingclip[x] >= floorclip[x]) continue; // Column already fully occluded

        // Interpolate distance across segment
        int64_t dx = x - x1;
        fixed_t tz = tz1 + (fixed_t)(((int64_t)(tz2 - tz1) * dx) / span_w);
        if (tz <= 0) continue;

        // Unclamped front sector elevations for correct world-space texture mapping
        int raw_y_ceil  = CENTER_Y - (int)(((int64_t)(front->ceilingheight - viewz) * FOCAL_LEN) / tz);
        int raw_y_floor = CENTER_Y - (int)(((int64_t)(front->floorheight - viewz) * FOCAL_LEN) / tz);

        int y_ceil  = (raw_y_ceil < 0) ? 0 : raw_y_ceil;
        int y_floor = (raw_y_floor >= VIEWPORT_H) ? (VIEWPORT_H - 1) : raw_y_floor;

        // Texture X coordinate: tile by actual world line length (repeating every 64 units)
        int64_t wdx = FROM_FIXED(line->dx);
        int64_t wdy = FROM_FIXED(line->dy);
        int adx = (wdx < 0) ? -wdx : wdx;
        int ady = (wdy < 0) ? -wdy : wdy;
        int wall_len = (adx > ady) ? (adx + (3 * ady) / 8) : (ady + (3 * adx) / 8);
        if (wall_len <= 0) wall_len = 64;
        int texX = (int)(((int64_t)dx * wall_len) / span_w) & 63;

        // 1. One-Sided Solid Wall
        if (!is_twosided) {
            // Draw ceiling above wall (DOOM 2 index 112)
            for (int y = 0; y < y_ceil; y++) {
                if (y > ceilingclip[x] && y < floorclip[x]) {
                    vram[y * SCREEN_W + x] = 112; // Authentic tech blue ceiling
                }
            }

            // Draw solid wall texture (Authentic DOOM 2 STARTAN3 tiled 64x64)
            int raw_wall_h = raw_y_floor - raw_y_ceil;
            int world_wall_h = FROM_FIXED(front->ceilingheight - front->floorheight);
            if (raw_wall_h > 0 && world_wall_h > 0) {
                for (int y = y_ceil; y <= y_floor; y++) {
                    if (y > ceilingclip[x] && y < floorclip[x]) {
                        int texY = (int)(((int64_t)(y - raw_y_ceil) * world_wall_h) / raw_wall_h) & 63;
                        vram[y * SCREEN_W + x] = tex_startan3[texY * 64 + texX];
                    }
                }
            }

            // Draw floor below wall (DOOM 2 index 72 brown stone)
            for (int y = y_floor + 1; y < VIEWPORT_H; y++) {
                if (y > ceilingclip[x] && y < floorclip[x]) {
                    vram[y * SCREEN_W + x] = 72; // Authentic brown stone floor flat
                }
            }

            // Solid wall occludes entire column
            floorclip[x] = -1;
            ceilingclip[x] = VIEWPORT_H;
            wall_dist_buffer[x] = tz;
        }
        // 2. Two-Sided Portal / Step Wall
        else {
            int raw_b_ceil  = CENTER_Y - (int)(((int64_t)(back->ceilingheight - viewz) * FOCAL_LEN) / tz);
            int raw_b_floor = CENTER_Y - (int)(((int64_t)(back->floorheight - viewz) * FOCAL_LEN) / tz);
            int y_b_ceil  = (raw_b_ceil < 0) ? 0 : raw_b_ceil;
            int y_b_floor = (raw_b_floor >= VIEWPORT_H) ? (VIEWPORT_H - 1) : raw_b_floor;

            // Draw ceiling above front sector
            for (int y = 0; y < y_ceil; y++) {
                if (y > ceilingclip[x] && y < floorclip[x]) {
                    vram[y * SCREEN_W + x] = 112; // Authentic tech blue ceiling
                }
            }

            // Upper texture (step down ceiling)
            if (front->ceilingheight > back->ceilingheight) {
                int raw_upper_h = raw_b_ceil - raw_y_ceil;
                int world_upper_h = FROM_FIXED(front->ceilingheight - back->ceilingheight);
                if (raw_upper_h > 0 && world_upper_h > 0) {
                    for (int y = y_ceil; y <= y_b_ceil; y++) {
                        if (y > ceilingclip[x] && y < floorclip[x]) {
                            int texY = (int)(((int64_t)(y - raw_y_ceil) * world_upper_h) / raw_upper_h) & 63;
                            vram[y * SCREEN_W + x] = tex_startan3[texY * 64 + texX];
                        }
                    }
                }
            }

            // Lower texture (STAIRS / Step up floor!)
            if (back->floorheight > front->floorheight) {
                int raw_lower_h = raw_y_floor - raw_b_floor;
                int world_lower_h = FROM_FIXED(back->floorheight - front->floorheight);
                if (raw_lower_h > 0 && world_lower_h > 0) {
                    for (int y = y_b_floor; y <= y_floor; y++) {
                        if (y > ceilingclip[x] && y < floorclip[x]) {
                            int texY = (int)(((int64_t)(y - raw_b_floor) * world_lower_h) / raw_lower_h) & 63;
                            vram[y * SCREEN_W + x] = tex_startan3[texY * 64 + texX];
                        }
                    }
                }
            }

            // Draw floor below front sector (e.g. floor of Hangar in front of stairs!)
            for (int y = y_floor + 1; y < VIEWPORT_H; y++) {
                if (y > ceilingclip[x] && y < floorclip[x]) {
                    vram[y * SCREEN_W + x] = 72; // Authentic brown stone floor flat
                }
            }

            // Update aperture clipping for two-sided portal
            if (y_ceil > ceilingclip[x])   ceilingclip[x] = y_ceil;
            if (y_b_ceil > ceilingclip[x]) ceilingclip[x] = y_b_ceil;
            if (y_floor < floorclip[x])    floorclip[x] = y_floor;
            if (y_b_floor < floorclip[x])  floorclip[x] = y_b_floor;
        }
    }
}

// ------------------------------------------------------------------------------
// Fill Backdrop: 100% Column Coverage for Open Ceiling / Floor Apertures
// Ensures every single pixel in the 320x168 viewport is overwritten directly
// with its authentic background flat, completely eliminating frame retention
// and buffer artifacts without needing a full-screen clear.
// ------------------------------------------------------------------------------
void R_FillBackdrop(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    for (int x = 0; x < SCREEN_W; x++) {
        int c_top = ceilingclip[x];
        int f_bot = floorclip[x];

        if (c_top < f_bot) {
            // Fill unoccluded ceiling above horizon (CENTER_Y = 84)
            int y_end_ceil = (f_bot < CENTER_Y) ? f_bot : CENTER_Y;
            for (int y = c_top + 1; y < y_end_ceil; y++) {
                vram[y * SCREEN_W + x] = 112; // Authentic tech blue ceiling
            }

            // Fill unoccluded floor below horizon down to f_bot
            int y_start_floor = (c_top + 1 > CENTER_Y) ? (c_top + 1) : CENTER_Y;
            for (int y = y_start_floor; y < f_bot; y++) {
                vram[y * SCREEN_W + x] = 72;  // Authentic brown stone floor flat
            }
        }
    }
}
