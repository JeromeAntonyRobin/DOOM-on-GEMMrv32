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

    int mid_tex = line->sidenum[0] ? line->sidenum[0]->midtexture : 0;
    int top_tex = line->sidenum[0] ? line->sidenum[0]->toptexture : 0;
    int bot_tex = line->sidenum[0] ? line->sidenum[0]->bottomtexture : 0;

    for (int x = start_x; x <= end_x; x++) {
        // Interpolate distance across segment
        int64_t dx = x - x1;
        fixed_t tz = tz1 + (fixed_t)(((int64_t)(tz2 - tz1) * dx) / span_w);
        if (tz <= 0) continue;

        // Front sector elevations (64-bit precision)
        int y_ceil  = CENTER_Y - (int)(((int64_t)(front->ceilingheight - viewz) * FOCAL_LEN) / tz);
        int y_floor = CENTER_Y - (int)(((int64_t)(front->floorheight - viewz) * FOCAL_LEN) / tz);

        // Clamp to screen bounds
        if (y_ceil < 0) y_ceil = 0;
        if (y_floor >= VIEWPORT_H) y_floor = VIEWPORT_H - 1;

        // Texture X coordinate
        int texX = (int)((dx * 64) / span_w) % 64;
        if (texX < 0) texX += 64;

        // Distance shading (0 = bright, 3 = darkest)
        int shade = FROM_FIXED(tz) / 100;
        if (shade > 3) shade = 3;

        // 1. One-Sided Solid Wall
        if (!is_twosided) {
            // Draw ceiling up to top of wall (DOOM 2 index 112)
            for (int y = 0; y < y_ceil; y++) {
                if (y > ceilingclip[x] && y < floorclip[x]) {
                    vram[y * SCREEN_W + x] = 112; // Authentic tech blue ceiling
                }
            }

            // Draw solid wall texture (Authentic DOOM 2 STARTAN3 64x64)
            int wall_h = y_floor - y_ceil;
            if (wall_h > 0) {
                for (int y = y_ceil; y <= y_floor; y++) {
                    if (y > ceilingclip[x] && y < floorclip[x]) {
                        int texY = ((y - y_ceil) * 64) / wall_h;
                        if (texY < 0) texY = 0;
                        if (texY >= 64) texY = 63;
                        uint8_t col = tex_startan3[texY * 64 + texX];
                        vram[y * SCREEN_W + x] = col;
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
            int y_b_ceil  = CENTER_Y - (int)(((int64_t)(back->ceilingheight - viewz) * FOCAL_LEN) / tz);
            int y_b_floor = CENTER_Y - (int)(((int64_t)(back->floorheight - viewz) * FOCAL_LEN) / tz);

            // Upper texture (step down ceiling)
            if (front->ceilingheight > back->ceilingheight) {
                int upper_h = y_b_ceil - y_ceil;
                if (upper_h > 0) {
                    for (int y = y_ceil; y <= y_b_ceil; y++) {
                        if (y > ceilingclip[x] && y < floorclip[x]) {
                            int texY = ((y - y_ceil) * 64) / upper_h;
                            if (texY < 0) texY = 0;
                            if (texY >= 64) texY = 63;
                            vram[y * SCREEN_W + x] = tex_startan3[texY * 64 + texX];
                        }
                    }
                }
                if (y_b_ceil > ceilingclip[x]) ceilingclip[x] = y_b_ceil;
            }

            // Lower texture (STAIRS / Step up floor!)
            if (back->floorheight > front->floorheight) {
                int lower_h = y_floor - y_b_floor;
                if (lower_h > 0) {
                    for (int y = y_b_floor; y <= y_floor; y++) {
                        if (y > ceilingclip[x] && y < floorclip[x]) {
                            int texY = ((y - y_b_floor) * 64) / lower_h;
                            if (texY < 0) texY = 0;
                            if (texY >= 64) texY = 63;
                            vram[y * SCREEN_W + x] = tex_startan3[texY * 64 + texX];
                        }
                    }
                }
                if (y_b_floor < floorclip[x]) floorclip[x] = y_b_floor;
            }

            if (tz < wall_dist_buffer[x]) wall_dist_buffer[x] = tz;
        }
    }
}
