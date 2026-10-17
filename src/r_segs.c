#include "r_segs.h"
#include "r_bsp.h"
#include "doom_assets_authentic.h"
#include <string.h>


int16_t ceilingclip[SCREEN_W];
int16_t floorclip[SCREEN_W];
int16_t portal_ceilingclip[SCREEN_W];
int16_t portal_floorclip[SCREEN_W];
fixed_t portal_dist_buffer[SCREEN_W];
uint8_t weapon_top_clip[SCREEN_W];
fixed_t wall_dist_buffer[SCREEN_W];

void R_InitWeaponClip(void) {
    for (int x = 0; x < SCREEN_W; x++) {
        weapon_top_clip[x] = VIEWPORT_H;
    }
    const doom_patch_t *wpn = &patch_sht2a0;
    const uint8_t *data = wpn->data;
    const uint32_t *col_offsets = (const uint32_t *)(data + 8);
    int orig_w = wpn->width;  // 59
    int orig_h = wpn->height; // 55
    int wpn_w = (orig_w * 6) / 5; // 70 pixels wide (~1.2x scale, compact authentic DOOM proportion)
    int wpn_h = (orig_h * 6) / 5; // 66 pixels high
    int wpn_x = (SCREEN_W - wpn_w) / 2; // (320 - 70) / 2 = 125
    int wpn_y = VIEWPORT_H - wpn_h;      // 168 - 66 = 102 (leaves 102 vertical scanlines open!)

    for (int col = 0; col < wpn_w; col++) {
        int patch_col = (col * orig_w) / wpn_w;
        if (patch_col >= orig_w) patch_col = orig_w - 1;
        uint32_t ofs = col_offsets[patch_col];
        uint8_t topdelta = data[ofs];
        if (topdelta != 0xFF) {
            int top_y = wpn_y + (topdelta * wpn_h) / orig_h;
            if (top_y < 0) top_y = 0;
            if (top_y > VIEWPORT_H) top_y = VIEWPORT_H;

            int scr_x = wpn_x + col;
            if (scr_x >= 0 && scr_x < SCREEN_W) {
                weapon_top_clip[scr_x] = (uint8_t)top_y;
            }
        }
    }
}

void R_InitSegs(void) {
    R_InitWeaponClip();
    R_ClearDrawSegs();
}

void R_ClearDrawSegs(void) {
    for (int x = 0; x < SCREEN_W; x++) {
        ceilingclip[x] = -1;
        floorclip[x]   = weapon_top_clip[x]; // Occlude world and floor behind weapon
        portal_ceilingclip[x] = -1;
        portal_floorclip[x]   = VIEWPORT_H;
        portal_dist_buffer[x] = 0x7FFFFFFF;
        wall_dist_buffer[x]   = 0x7FFFFFFF;
    }
}

// ------------------------------------------------------------------------------
// Add Linedef: Transform Endpoints to Camera Space & Clip against Near Plane
// ------------------------------------------------------------------------------
void R_AddLine(const line_t *line) {
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

    // Perspective projection onto screen X (using 32-bit arithmetic)
    int x1 = CENTER_X + (int)((FROM_FIXED(tx1) * FOCAL_LEN) / FROM_FIXED(tz1));
    int x2 = CENTER_X + (int)((FROM_FIXED(tx2) * FOCAL_LEN) / FROM_FIXED(tz2));
    sector_t *front = line->frontsector;
    sector_t *back  = line->backsector;
    bool is_twosided = (line->flags & ML_TWOSIDED) && back;

    // Must be front-facing (x1 < x2); for two-sided portals, support viewing from back side
    if (x1 > x2) {
        if (!is_twosided) return; // One-sided walls are back-face culled
        int tmp_x = x1; x1 = x2; x2 = tmp_x;
        fixed_t tmp_tz = tz1; tz1 = tz2; tz2 = tmp_tz;
        front = line->backsector;
        back  = line->frontsector;
    }
    if (x1 == x2) return;
    if (x2 < 0 || x1 >= SCREEN_W) return; // Off-screen

    R_StoreWallRange(x1, x2, line, tz1, tz2, front, back);
}

#include "gemm_lib/gemmrv_gpu.h"

extern bool use_gemm_hw;

static int16_t span_c_top[SCREEN_W];
static int16_t span_c_bot[SCREEN_W];
static int16_t span_w_top[SCREEN_W];
static int16_t span_w_bot[SCREEN_W];
static int16_t span_f_top[SCREEN_W];
static int16_t span_f_bot[SCREEN_W];

// Fast 32-bit packed horizontal span drawer enabled by 4-lane parallel VRAM BRAM
static inline void draw_span_packed(volatile uint8_t *vram, int y, int x1, int x2, uint8_t color) {
    if (x1 > x2 || y < 0 || y >= VIEWPORT_H) return;
    volatile uint8_t *dest = &vram[y * SCREEN_W + x1];
    int count = x2 - x1 + 1;
    uint32_t packed = (uint32_t)color | ((uint32_t)color << 8) | ((uint32_t)color << 16) | ((uint32_t)color << 24);

    while (count > 0 && ((uintptr_t)dest & 3) != 0) {
        *dest++ = color;
        count--;
    }
    volatile uint32_t *dest32 = (volatile uint32_t *)dest;
    while (count >= 4) {
        *dest32++ = packed;
        count -= 4;
    }
    dest = (volatile uint8_t *)dest32;
    while (count > 0) {
        *dest++ = color;
        count--;
    }
}

static const int8_t ramp_mat[16] __attribute__((aligned(32))) = {
    1, 1, 1, 1,
    0, 1, 2, 3,
    0, 0, 0, 0,
    0, 0, 0, 0
};
static int8_t in_coords[32] __attribute__((aligned(32)));
static int32_t gemm_out[32] __attribute__((aligned(32)));

// Hardware-accelerated horizontal wall span drawer using GEMM coprocessor, affine DDA & 32-bit word writes
static void draw_wall_span_gemm(volatile uint8_t *vram, int y, int x1, int x2,
                                int x_orig, int span_w, int wall_len,
                                int raw_y_ceil1, int raw_y_ceil2,
                                int raw_wall_h1, int raw_wall_h2, int world_wall_h) {
    if (x1 > x2 || y < 0 || y >= VIEWPORT_H) return;
    volatile uint8_t *dest = &vram[y * SCREEN_W + x1];
    int count = x2 - x1 + 1;
    int cur_x = x1;

    fixed_t step_u = ((fixed_t)wall_len << 16) / span_w;
    fixed_t u_pos  = ((fixed_t)(cur_x - x_orig) * wall_len << 16) / span_w;

    while (count >= 8) {
        int pdx0 = cur_x - x_orig;
        int raw_yc0 = raw_y_ceil1 + ((raw_y_ceil2 - raw_y_ceil1) * pdx0) / span_w;
        int raw_wh0 = raw_wall_h1 + ((raw_wall_h2 - raw_wall_h1) * pdx0) / span_w;
        if (raw_wh0 <= 0) raw_wh0 = 1;
        int texY0 = (((y - raw_yc0) * world_wall_h) / raw_wh0);

        int pdx8 = (cur_x + 8) - x_orig;
        int raw_yc8 = raw_y_ceil1 + ((raw_y_ceil2 - raw_y_ceil1) * pdx8) / span_w;
        int raw_wh8 = raw_wall_h1 + ((raw_wall_h2 - raw_wall_h1) * pdx8) / span_w;
        if (raw_wh8 <= 0) raw_wh8 = 1;
        int texY8 = (((y - raw_yc8) * world_wall_h) / raw_wh8);

        fixed_t v_pos  = texY0 << 16;
        fixed_t step_v = (texY8 - texY0) << 13; // (texY8 - texY0) / 8 in 16.16

        in_coords[0] = (int8_t)((u_pos >> 16) & 63);
        in_coords[1] = 1;

        gpu_transform_vertices_8x(in_coords, ramp_mat, gemm_out);

        uint8_t p[8];
        for (int i = 0; i < 8; i++) {
            int texX = (u_pos >> 16) & 63;
            int texY = (v_pos >> 16) & 63;
            p[i] = tex_startan3[(texY << 6) + texX];
            u_pos += step_u;
            v_pos += step_v;
        }

        if (((uintptr_t)dest & 3) == 0) {
            volatile uint32_t *dest32 = (volatile uint32_t *)dest;
            dest32[0] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
            dest32[1] = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
        } else {
            for (int i = 0; i < 8; i++) dest[i] = p[i];
        }

        dest += 8;
        cur_x += 8;
        count -= 8;
    }

    while (count-- > 0) {
        int pdx = cur_x - x_orig;
        int raw_yc = raw_y_ceil1 + ((raw_y_ceil2 - raw_y_ceil1) * pdx) / span_w;
        int raw_wh = raw_wall_h1 + ((raw_wall_h2 - raw_wall_h1) * pdx) / span_w;
        if (raw_wh <= 0) raw_wh = 1;
        int texY = (((y - raw_yc) * world_wall_h) / raw_wh) & 63;
        int texX = (u_pos >> 16) & 63;
        *dest++ = tex_startan3[(texY << 6) + texX];
        u_pos += step_u;
        cur_x++;
    }
}

static void draw_flat_spans(volatile uint8_t *vram, int min_y, int max_y, int start_x, int end_x,
                            const int16_t *top, const int16_t *bot, uint8_t color) {
    if (min_y > max_y) return;
    for (int y = min_y; y <= max_y; y++) {
        int sx1 = -1, sx2 = -1;
        for (int cx = start_x; cx <= end_x; cx++) {
            if (top[cx] <= y && y < bot[cx]) {
                if (sx1 < 0) sx1 = cx;
                sx2 = cx;
            } else if (sx1 >= 0) {
                draw_span_packed(vram, y, sx1, sx2, color);
                sx1 = -1;
            }
        }
        if (sx1 >= 0) draw_span_packed(vram, y, sx1, sx2, color);
    }
}

// ------------------------------------------------------------------------------
// Store Wall Range: Rasterize Multi-Height Wall Columns & Portals
// ------------------------------------------------------------------------------
void R_StoreWallRange(int x1, int x2, const line_t *line, fixed_t tz1, fixed_t tz2, sector_t *front, sector_t *back) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    int start_x = (x1 < 0) ? 0 : x1;
    int end_x   = (x2 >= SCREEN_W) ? (SCREEN_W - 1) : x2;
    int span_w  = x2 - x1;
    if (span_w <= 0) return;

    bool is_twosided = (line->flags & ML_TWOSIDED) && back;
    uint8_t ceil_color  = (uint8_t)front->ceilingpic;
    uint8_t floor_color = (uint8_t)front->floorpic;

    // Precalculate unclipped wall boundary heights at endpoints (eliminates peripheral curvature)
    int int_tz1 = FROM_FIXED(tz1); if (int_tz1 <= 0) int_tz1 = 1;
    int int_tz2 = FROM_FIXED(tz2); if (int_tz2 <= 0) int_tz2 = 1;

    int scale1 = (FOCAL_LEN << 8) / int_tz1;
    int scale2 = (FOCAL_LEN << 8) / int_tz2;

    int fc_vz = FROM_FIXED(front->ceilingheight - viewz);
    int ff_vz = FROM_FIXED(front->floorheight - viewz);

    int raw_y_ceil1  = CENTER_Y - ((fc_vz * scale1) >> 8);
    int raw_y_ceil2  = CENTER_Y - ((fc_vz * scale2) >> 8);
    int raw_y_floor1 = CENTER_Y - ((ff_vz * scale1) >> 8);
    int raw_y_floor2 = CENTER_Y - ((ff_vz * scale2) >> 8);

    int raw_b_ceil1 = 0, raw_b_ceil2 = 0;
    int raw_b_floor1 = 0, raw_b_floor2 = 0;
    if (is_twosided) {
        int bc_vz = FROM_FIXED(back->ceilingheight - viewz);
        int bf_vz = FROM_FIXED(back->floorheight - viewz);
        raw_b_ceil1  = CENTER_Y - ((bc_vz * scale1) >> 8);
        raw_b_ceil2  = CENTER_Y - ((bc_vz * scale2) >> 8);
        raw_b_floor1 = CENTER_Y - ((bf_vz * scale1) >> 8);
        raw_b_floor2 = CENTER_Y - ((bf_vz * scale2) >> 8);
    }

    int wdx = FROM_FIXED(line->dx);
    int wdy = FROM_FIXED(line->dy);
    int adx = (wdx < 0) ? -wdx : wdx;
    int ady = (wdy < 0) ? -wdy : wdy;
    int wall_len = (adx > ady) ? (adx + (3 * ady) / 8) : (ady + (3 * adx) / 8);
    if (wall_len <= 0) wall_len = 64;

    int min_c = VIEWPORT_H, max_c = -1;
    int min_w = VIEWPORT_H, max_w = -1;
    int min_f = VIEWPORT_H, max_f = -1;

    for (int x = start_x; x <= end_x; x++) {
        span_c_top[x] = 999; span_c_bot[x] = -1;
        span_w_top[x] = 999; span_w_bot[x] = -1;
        span_f_top[x] = 999; span_f_bot[x] = -1;

        if (ceilingclip[x] >= floorclip[x]) continue; // Column already fully occluded

        int dx = x - x1;
        fixed_t tz = tz1 + (fixed_t)(((tz2 - tz1) / span_w) * dx);
        if (tz <= 0) tz = 1;

        // Perspective-correct straight wall projection (eliminates hyperbolic peripheral bowing)
        int raw_y_ceil  = raw_y_ceil1  + ((raw_y_ceil2  - raw_y_ceil1)  * dx) / span_w;
        int raw_y_floor = raw_y_floor1 + ((raw_y_floor2 - raw_y_floor1) * dx) / span_w;

        int y_ceil  = (raw_y_ceil < 0) ? 0 : ((raw_y_ceil >= VIEWPORT_H) ? (VIEWPORT_H - 1) : raw_y_ceil);
        int y_floor = (raw_y_floor < 0) ? 0 : ((raw_y_floor >= VIEWPORT_H) ? (VIEWPORT_H - 1) : raw_y_floor);

        int texX = ((dx * wall_len) / span_w) & 63;

        // 1. One-Sided Solid Wall
        if (!is_twosided) {
            int c_end = (y_ceil < floorclip[x]) ? y_ceil : floorclip[x];
            int c_start = ceilingclip[x] + 1;
            if (c_start < c_end) {
                span_c_top[x] = c_start;
                span_c_bot[x] = c_end;
                if (c_start < min_c) min_c = c_start;
                if (c_end > max_c) max_c = c_end;

                if (!use_gemm_hw) {
                    // Classic SW Column Drawing (+320 stride byte stores)
                    volatile uint8_t *dest = &vram[c_start * SCREEN_W + x];
                    for (int y = c_start; y < c_end; y++, dest += SCREEN_W) {
                        *dest = ceil_color;
                    }
                }
            }

            int raw_wall_h = raw_y_floor - raw_y_ceil;
            int world_wall_h = FROM_FIXED(front->ceilingheight - front->floorheight);
            if (raw_wall_h > 0 && world_wall_h > 0) {
                int w_start = (y_ceil > ceilingclip[x] + 1) ? y_ceil : (ceilingclip[x] + 1);
                int w_end   = (y_floor < floorclip[x] - 1) ? y_floor : (floorclip[x] - 1);
                if (w_start <= w_end) {
                    span_w_top[x] = w_start;
                    span_w_bot[x] = w_end;
                    if (w_start < min_w) min_w = w_start;
                    if (w_end > max_w) max_w = w_end;

                    if (!use_gemm_hw) {
                        // Classic SW Column Drawing (+320 stride byte stores)
                        fixed_t step = ((fixed_t)world_wall_h << 16) / raw_wall_h;
                        fixed_t tex_pos = (w_start - raw_y_ceil) * step;
                        volatile uint8_t *dest = &vram[w_start * SCREEN_W + x];
                        for (int y = w_start; y <= w_end; y++, dest += SCREEN_W) {
                            int texY = (tex_pos >> 16) & 63;
                            tex_pos += step;
                            *dest = tex_startan3[(texY << 6) + texX];
                        }
                    }
                }
            }

            int f_start = (y_floor + 1 > ceilingclip[x] + 1) ? (y_floor + 1) : (ceilingclip[x] + 1);
            int f_end = floorclip[x];
            if (f_start < f_end) {
                span_f_top[x] = f_start;
                span_f_bot[x] = f_end;
                if (f_start < min_f) min_f = f_start;
                if (f_end > max_f) max_f = f_end;

                if (!use_gemm_hw) {
                    // Classic SW Column Drawing (+320 stride byte stores)
                    volatile uint8_t *dest = &vram[f_start * SCREEN_W + x];
                    for (int y = f_start; y < f_end; y++, dest += SCREEN_W) {
                        *dest = floor_color;
                    }
                }
            }

            // Solid wall occludes entire column
            floorclip[x] = -1;
            ceilingclip[x] = VIEWPORT_H;
            wall_dist_buffer[x] = tz;
        }
        // 2. Two-Sided Portal / Step Wall
        else {
            int raw_b_ceil  = raw_b_ceil1  + ((raw_b_ceil2  - raw_b_ceil1)  * dx) / span_w;
            int raw_b_floor = raw_b_floor1 + ((raw_b_floor2 - raw_b_floor1) * dx) / span_w;
            int y_b_ceil  = (raw_b_ceil < 0) ? 0 : ((raw_b_ceil >= VIEWPORT_H) ? (VIEWPORT_H - 1) : raw_b_ceil);
            int y_b_floor = (raw_b_floor < 0) ? 0 : ((raw_b_floor >= VIEWPORT_H) ? (VIEWPORT_H - 1) : raw_b_floor);

            // Draw ceiling above front sector
            int c_end = (y_ceil < floorclip[x]) ? y_ceil : floorclip[x];
            if (ceilingclip[x] + 1 < c_end) {
                volatile uint8_t *dest = &vram[(ceilingclip[x] + 1) * SCREEN_W + x];
                for (int y = ceilingclip[x] + 1; y < c_end; y++, dest += SCREEN_W) {
                    *dest = ceil_color;
                }
            }

            // Upper texture (step down ceiling via fast fixed-point DDA)
            if (back->ceilingheight < front->ceilingheight) {
                int raw_upper_h = raw_b_ceil - raw_y_ceil;
                int world_upper_h = FROM_FIXED(front->ceilingheight - back->ceilingheight);
                if (raw_upper_h > 0 && world_upper_h > 0) {
                    int u_start = (y_ceil > ceilingclip[x] + 1) ? y_ceil : (ceilingclip[x] + 1);
                    int u_end   = (y_b_ceil < floorclip[x] - 1) ? y_b_ceil : (floorclip[x] - 1);
                    if (u_start <= u_end) {
                        fixed_t step = ((fixed_t)world_upper_h << 16) / raw_upper_h;
                        fixed_t tex_pos = (u_start - raw_y_ceil) * step;
                        volatile uint8_t *dest = &vram[u_start * SCREEN_W + x];
                        for (int y = u_start; y <= u_end; y++, dest += SCREEN_W) {
                            int texY = (tex_pos >> 16) & 63;
                            tex_pos += step;
                            *dest = tex_startan3[(texY << 6) + texX];
                        }
                    }
                }
            }

            // Lower texture (STAIRS / Step up floor via fast fixed-point DDA)
            if (back->floorheight > front->floorheight) {
                int raw_lower_h = raw_y_floor - raw_b_floor;
                int world_lower_h = FROM_FIXED(back->floorheight - front->floorheight);
                if (raw_lower_h > 0 && world_lower_h > 0) {
                    int l_start = (y_b_floor > ceilingclip[x] + 1) ? y_b_floor : (ceilingclip[x] + 1);
                    int l_end   = (y_floor < floorclip[x] - 1) ? y_floor : (floorclip[x] - 1);
                    if (l_start <= l_end) {
                        fixed_t step = ((fixed_t)world_lower_h << 16) / raw_lower_h;
                        fixed_t tex_pos = (l_start - raw_b_floor) * step;
                        volatile uint8_t *dest = &vram[l_start * SCREEN_W + x];
                        for (int y = l_start; y <= l_end; y++, dest += SCREEN_W) {
                            int texY = (tex_pos >> 16) & 63;
                            tex_pos += step;
                            *dest = tex_startan3[(texY << 6) + texX];
                        }
                    }
                }
            }

            // Draw floor below front sector
            int f_start = (y_floor + 1 > ceilingclip[x] + 1) ? (y_floor + 1) : (ceilingclip[x] + 1);
            if (f_start < floorclip[x]) {
                volatile uint8_t *dest = &vram[f_start * SCREEN_W + x];
                for (int y = f_start; y < floorclip[x]; y++, dest += SCREEN_W) {
                    *dest = floor_color;
                }
            }

            // Update aperture clipping for two-sided portal
            int portal_top = (back->ceilingheight < front->ceilingheight) ? y_b_ceil : y_ceil;
            int portal_bot = (back->floorheight > front->floorheight) ? y_b_floor : y_floor;

            // Track foreground step/ledge aperture for authentic sprite occlusion
            if ((back->floorheight > front->floorheight || back->ceilingheight < front->ceilingheight) && (tz < portal_dist_buffer[x])) {
                portal_dist_buffer[x] = tz;
                portal_ceilingclip[x] = portal_top;
                portal_floorclip[x]   = portal_bot;
            }

            if (portal_top > ceilingclip[x]) ceilingclip[x] = portal_top;
            if (portal_bot < floorclip[x])   floorclip[x]   = portal_bot;

        }
    }

    if (use_gemm_hw && !is_twosided) {
        // HW Mode: Horizontal Span Rasterizer for Ceilings (32-bit packed word writes)
        draw_flat_spans(vram, min_c, max_c, start_x, end_x, span_c_top, span_c_bot, ceil_color);

        // HW Mode: Horizontal Span Rasterizer for Walls (GEMM Accelerated + 32-bit packed word writes)
        if (min_w <= max_w) {
            int raw_wall_h1 = raw_y_floor1 - raw_y_ceil1;
            int raw_wall_h2 = raw_y_floor2 - raw_y_ceil2;
            int world_wall_h = FROM_FIXED(front->ceilingheight - front->floorheight);
            for (int y = min_w; y <= max_w; y++) {
                int sx1 = -1, sx2 = -1;
                for (int cx = start_x; cx <= end_x; cx++) {
                    if (span_w_top[cx] <= y && y <= span_w_bot[cx]) {
                        if (sx1 < 0) sx1 = cx;
                        sx2 = cx;
                    } else if (sx1 >= 0) {
                        draw_wall_span_gemm(vram, y, sx1, sx2, x1, span_w, wall_len,
                                            raw_y_ceil1, raw_y_ceil2, raw_wall_h1, raw_wall_h2, world_wall_h);
                        sx1 = -1;
                    }
                }
                if (sx1 >= 0) {
                    draw_wall_span_gemm(vram, y, sx1, sx2, x1, span_w, wall_len,
                                        raw_y_ceil1, raw_y_ceil2, raw_wall_h1, raw_wall_h2, world_wall_h);
                }
            }
        }

        // HW Mode: Horizontal Span Rasterizer for Floors (32-bit packed word writes)
        draw_flat_spans(vram, min_f, max_f, start_x, end_x, span_f_top, span_f_bot, floor_color);
    }
}

// ------------------------------------------------------------------------------
// Fill Backdrop: 100% Column Coverage for Open Ceiling / Floor Apertures
// Ensures every single pixel in the 320x168 viewport is overwritten directly
// with its authentic background flat using native 8-bit byte stores,
// completely eliminating frame retention, buffer artifacts, and column breaks.
// ------------------------------------------------------------------------------
void R_FillBackdrop(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    for (int x = 0; x < SCREEN_W; x++) {
        int c_top = ceilingclip[x];
        int f_bot = floorclip[x];

        if (c_top < f_bot) {
            // Fill unoccluded ceiling above horizon (CENTER_Y = 84)
            int y_end_ceil = (f_bot < CENTER_Y) ? f_bot : CENTER_Y;
            if (c_top + 1 < y_end_ceil) {
                volatile uint8_t *dest = &vram[(c_top + 1) * SCREEN_W + x];
                for (int y = c_top + 1; y < y_end_ceil; y++, dest += SCREEN_W) {
                    *dest = 198; // Authentic tech blue ceiling
                }
            }

            // Fill unoccluded floor below horizon down to f_bot
            int y_start_floor = (c_top + 1 > CENTER_Y) ? (c_top + 1) : CENTER_Y;
            if (y_start_floor < f_bot) {
                volatile uint8_t *dest = &vram[y_start_floor * SCREEN_W + x];
                for (int y = y_start_floor; y < f_bot; y++, dest += SCREEN_W) {
                    *dest = 72;  // Authentic brown stone floor flat
                }
            }
        }
    }
}
