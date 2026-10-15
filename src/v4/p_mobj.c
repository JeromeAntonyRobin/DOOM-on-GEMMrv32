#include "p_mobj.h"
#include "doom_map.h"
#include "doom_assets_4bpp.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "gemm_lib/gemmrv_gpu.h"
#include <string.h>

mobj_t mobjs[MAX_MOBJS];
int num_mobjs = 0;
mobj_t *player_mobj = NULL;

extern int player_health;
extern int player_ammo;
extern int player_score;

#define VIEWPORT_H 168
#define CENTER_X   (SCREEN_W / 2)
#define CENTER_Y   (VIEWPORT_H / 2)
#define FOCAL_LEN  160

// ------------------------------------------------------------------------------
// Initialize Mobjs from Map Spawn Table
// ------------------------------------------------------------------------------
void P_InitMobjs(void) {
    num_mobjs = 0;
    memset(mobjs, 0, sizeof(mobjs));

    for (size_t i = 0; i < NUM_SPAWNS; i++) {
        const mobj_spawn_t *s = &map_spawns[i];
        mobj_t *mo = P_SpawnMobj(s->x, s->y, s->z, s->type, s->angle);
        if (s->type == MT_PLAYER) {
            player_mobj = mo;
        }
    }
}

// ------------------------------------------------------------------------------
// Spawn Map Object
// ------------------------------------------------------------------------------
mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, angle_t angle) {
    if (num_mobjs >= MAX_MOBJS) return NULL;

    mobj_t *mo = &mobjs[num_mobjs++];
    mo->x = x;
    mo->y = y;
    mo->z = z;
    mo->momx = 0;
    mo->momy = 0;
    mo->momz = 0;
    mo->angle = angle;
    mo->type = type;
    mo->target = NULL;
    mo->flags = MF_SOLID | MF_SHOOTABLE;

    switch (type) {
        case MT_PLAYER:
            mo->health = 100;
            mo->radius = TO_FIXED(16);
            mo->height = TO_FIXED(56);
            break;
        case MT_IMP:
            mo->health = 60;
            mo->radius = TO_FIXED(20);
            mo->height = TO_FIXED(56);
            mo->state  = S_IMP_SPAWN;
            mo->tics   = 10;
            break;
        case MT_BARREL:
            mo->health = 20;
            mo->radius = TO_FIXED(10);
            mo->height = TO_FIXED(32);
            mo->state  = S_BARREL_IDLE;
            break;
        case MT_FIREBALL:
            mo->health = 1;
            mo->radius = TO_FIXED(8);
            mo->height = TO_FIXED(8);
            mo->flags  = MF_MISSILE;
            mo->state  = S_FIREBALL1;
            mo->tics   = 4;
            break;
        case MT_MEDKIT:
            mo->health = 1;
            mo->radius = TO_FIXED(12);
            mo->height = TO_FIXED(16);
            mo->flags  = 0; // Walk-through pickup
            break;
        default:
            break;
    }

    return mo;
}

// ------------------------------------------------------------------------------
// Remove Mobj
// ------------------------------------------------------------------------------
void P_RemoveMobj(mobj_t *mobj) {
    for (int i = 0; i < num_mobjs; i++) {
        if (&mobjs[i] == mobj) {
            mobjs[i] = mobjs[--num_mobjs];
            break;
        }
    }
}

// ------------------------------------------------------------------------------
// Monster AI Thinker: Imp State Machine & Fireball Spawning
// ------------------------------------------------------------------------------
static void P_ImpThinker(mobj_t *mo) {
    if (mo->health <= 0) {
        if (mo->state != S_IMP_DEAD) {
            mo->state = S_IMP_DEAD;
            player_score += 100;
        }
        return;
    }

    // Distance to player
    int d_x = FROM_FIXED(player_mobj->x - mo->x);
    int d_y = FROM_FIXED(player_mobj->y - mo->y);
    int dist_sq = d_x * d_x + d_y * d_y;

    // State 1: Idle spawn guard
    if (mo->state == S_IMP_SPAWN) {
        if (dist_sq < 400 * 400) {
            mo->state = S_IMP_SEE1; // Wake up!
            mo->target = player_mobj;
            mo->tics = 3;
        }
    }
    // State 2: Chasing player
    else if (mo->state >= S_IMP_SEE1 && mo->state <= S_IMP_SEE4) {
        if (--mo->tics <= 0) {
            mo->state = (mo->state == S_IMP_SEE4) ? S_IMP_SEE1 : (mo->state + 1);
            mo->tics = 4;

            // Step toward player
            fixed_t speed = TO_FIXED(3);
            if (d_x > 20)  mo->x += speed;
            if (d_x < -20) mo->x -= speed;
            if (d_y > 20)  mo->y += speed;
            if (d_y < -20) mo->y -= speed;

            // Attack chance if in range
            if (dist_sq < 250 * 250 && dist_sq > 60 * 60) {
                mo->state = S_IMP_ATK1;
                mo->tics = 10;
            }
        }
    }
    // State 3: Attack (Spawns a flying fireball toward player!)
    else if (mo->state == S_IMP_ATK1) {
        if (--mo->tics <= 0) {
            mo->state = S_IMP_SEE1;
            // Spawn fireball projectile
            mobj_t *fb = P_SpawnMobj(mo->x, mo->y, mo->z + TO_FIXED(32), MT_FIREBALL, 0);
            if (fb) {
                fixed_t fspeed = TO_FIXED(6);
                fb->momx = (d_x > 0) ? fspeed : -fspeed;
                fb->momy = (d_y > 0) ? fspeed : -fspeed;
            }
        }
    }
}

// ------------------------------------------------------------------------------
// Fireball Projectile Physics Thinker
// ------------------------------------------------------------------------------
static void P_FireballThinker(mobj_t *mo) {
    mo->x += mo->momx;
    mo->y += mo->momy;

    // Check collision with player
    fixed_t pdx = player_mobj->x - mo->x;
    fixed_t pdy = player_mobj->y - mo->y;
    if (pdx > TO_FIXED(-24) && pdx < TO_FIXED(24) &&
        pdy > TO_FIXED(-24) && pdy < TO_FIXED(24)) {
        player_health -= 15;
        if (player_health < 0) player_health = 0;
        P_RemoveMobj(mo); // Detonate on player!
    }
}

// ------------------------------------------------------------------------------
// Global Mobj Thinker Coordinator (35 Hz Tick)
// ------------------------------------------------------------------------------
void P_MobjThinker(mobj_t *mo) {
    switch (mo->type) {
        case MT_IMP:
            P_ImpThinker(mo);
            break;
        case MT_FIREBALL:
            P_FireballThinker(mo);
            break;
        default:
            break;
    }
}

void P_Ticker(void) {
    for (int i = 0; i < num_mobjs; i++) {
        P_MobjThinker(&mobjs[i]);
    }
}

// ------------------------------------------------------------------------------
// Render 2.5D Billboarded Sprites (Imps, Barrels, Fireballs, Medkits)
// Depth-tested against wall_dist_buffer[x]
// ------------------------------------------------------------------------------
void R_DrawMasked(void) {
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;

    // Feed up to 8 entities to GEMM coprocessor for hardware batch transformation
    int8_t in_coords[8 * 4] __attribute__((aligned(32))) = {0};
    int8_t view_matrix[4 * 4] __attribute__((aligned(32))) = {0};
    int32_t gemm_out[8 * 4] __attribute__((aligned(32))) = {0};

    int entity_count = (num_mobjs > 8) ? 8 : num_mobjs;
    for (int i = 0; i < entity_count; i++) {
        in_coords[i * 4 + 0] = (int8_t)(FROM_FIXED(mobjs[i].x - viewx));
        in_coords[i * 4 + 1] = (int8_t)(FROM_FIXED(mobjs[i].y - viewy));
        in_coords[i * 4 + 2] = (int8_t)(FROM_FIXED(mobjs[i].z - viewz));
        in_coords[i * 4 + 3] = 1;
    }

    // View matrix
    int8_t c_cos = (int8_t)(FROM_FIXED(viewcos * 64));
    int8_t c_sin = (int8_t)(FROM_FIXED(viewsin * 64));
    view_matrix[0 * 4 + 0] = c_cos;
    view_matrix[0 * 4 + 1] = -c_sin;
    view_matrix[1 * 4 + 0] = c_sin;
    view_matrix[1 * 4 + 1] = c_cos;
    view_matrix[2 * 4 + 2] = 64;
    view_matrix[3 * 4 + 3] = 64;

    gpu_transform_vertices_8x(in_coords, view_matrix, gemm_out);

    // Render sprites
    for (int i = 0; i < num_mobjs; i++) {
        mobj_t *mo = &mobjs[i];
        if (mo->type == MT_PLAYER) continue; // Don't draw player sprite on top of player camera

        fixed_t px = mo->x - viewx;
        fixed_t py = mo->y - viewy;

        fixed_t tz = FIXED_MUL(px, viewcos) + FIXED_MUL(py, viewsin);
        fixed_t tx = FIXED_MUL(px, viewsin) - FIXED_MUL(py, viewcos);

        if (tz <= TO_FIXED(8)) continue; // Behind or too close

        int screenX = CENTER_X + (int)(((int64_t)tx * FOCAL_LEN) / tz);
        int spr_h   = (int)(((int64_t)VIEWPORT_H * 32 * FP_ONE) / tz);
        if (spr_h <= 0) continue;
        if (spr_h > VIEWPORT_H) spr_h = VIEWPORT_H;
        int spr_w   = spr_h;

        int drawStartX = screenX - spr_w / 2;
        int drawEndX   = screenX + spr_w / 2;
        int drawStartY = CENTER_Y - (int)(((int64_t)(mo->z - viewz + TO_FIXED(24)) * FOCAL_LEN) / tz);
        int drawEndY   = drawStartY + spr_h;

        if (drawStartX < 0) drawStartX = 0;
        if (drawEndX >= SCREEN_W) drawEndX = SCREEN_W - 1;
        if (drawStartY < 0) drawStartY = 0;
        if (drawEndY >= VIEWPORT_H) drawEndY = VIEWPORT_H - 1;

        // Select sprite frame
        uint8_t *spr_data = spr_imp_walk[0];
        if (mo->type == MT_IMP) {
            if (mo->state == S_IMP_ATK1)     spr_data = spr_imp_attack;
            else if (mo->state == S_IMP_DEAD) spr_data = spr_imp_die[0];
            else spr_data = spr_imp_walk[(mo->tics % 2 == 0) ? 0 : 1];
        } else if (mo->type == MT_FIREBALL) {
            spr_data = spr_fireball;
        } else if (mo->type == MT_BARREL) {
            spr_data = spr_barrel;
        } else if (mo->type == MT_MEDKIT) {
            spr_data = spr_medkit;
        }

        // Draw vertical sprite slices
        for (int x = drawStartX; x <= drawEndX; x++) {
            // Depth test against Z-Buffer: if occluded by wall, skip!
            if (tz >= wall_dist_buffer[x]) continue;

            int texX = ((x - (screenX - spr_w / 2)) * SPR32_W) / spr_w;
            if (texX < 0) texX = 0;
            if (texX >= SPR32_W) texX = SPR32_W - 1;

            for (int y = drawStartY; y <= drawEndY; y++) {
                int texY = ((y - drawStartY) * SPR32_H) / spr_h;
                if (texY < 0) texY = 0;
                if (texY >= SPR32_H) texY = SPR32_H - 1;

                int byte_idx = (texY * SPR32_W + texX) >> 1;
                uint8_t col = (texX & 1) ? (spr_data[byte_idx] & 0x0F) : (spr_data[byte_idx] >> 4);

                if (col != COLOR_4BPP_TRANS) {
                    put_pixel_4bpp(vram, x, y, col);
                }
            }
        }
    }
}
