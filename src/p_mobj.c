#include "p_mobj.h"
#include "doom_map.h"
#include "doom_assets_authentic.h"
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
        case MT_DEMON:
            mo->health = 150;
            mo->radius = TO_FIXED(30);
            mo->height = TO_FIXED(56);
            mo->state  = S_IMP_SPAWN;
            mo->tics   = 10;
            break;
        case MT_SHOTGUNGUY:
            mo->health = 30;
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

        // Select authentic DOOM 2 monster patch
        const doom_patch_t *patch = NULL;
        if (mo->type == MT_IMP) {
            patch = &patch_trooa1;
        } else if (mo->type == MT_SHOTGUNGUY || mo->type == MT_ZOMBIEMAN) {
            patch = &patch_sposa1;
        } else if (mo->type == MT_DEMON) {
            patch = &patch_sarga1;
        } else {
            continue; // Skip any entities without authentic DOOM 2 patches
        }

        // Exact perspective projection aligned to floor
        int screenX = CENTER_X + (int)(((int64_t)tx * FOCAL_LEN) / tz);
        int top_z = mo->z + mo->height - viewz;
        int bot_z = mo->z - viewz;

        int drawStartY = CENTER_Y - (int)(((int64_t)top_z * FOCAL_LEN) / tz);
        int drawEndY   = CENTER_Y - (int)(((int64_t)bot_z * FOCAL_LEN) / tz);
        int spr_h      = drawEndY - drawStartY;
        if (spr_h <= 0) continue;

        // Preserve authentic aspect ratio from patch dimensions
        int spr_w = (patch->height > 0) ? ((spr_h * patch->width) / patch->height) : spr_h;
        if (spr_w <= 0) spr_w = 1;

        draw_authentic_patch_scaled(vram, patch, screenX, drawStartY, spr_w, spr_h, tz);
    }
}
