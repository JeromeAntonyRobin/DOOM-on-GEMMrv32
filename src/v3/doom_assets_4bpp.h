#ifndef DOOM_ASSETS_4BPP_H
#define DOOM_ASSETS_4BPP_H

#include <stdint.h>
#include "doom_defs.h"

// ------------------------------------------------------------------------------
// 4bpp Texture & Asset Dimensions
// ------------------------------------------------------------------------------
#define TEX64_W             64
#define TEX64_H             64
#define TEX64_BYTES         ((TEX64_W * TEX64_H) / 2) // 2048 bytes per 64x64 4bpp texture
#define NUM_WALL_TEX        4 // 0: STARTAN, 1: BROWN96, 2: COMP01, 3: DOOR3

#define FLAT32_W            32
#define FLAT32_H            32
#define FLAT32_BYTES        ((FLAT32_W * FLAT32_H) / 2) // 512 bytes per 32x32 4bpp flat
#define NUM_FLATS           3 // 0: FLOOR_STONE, 1: NUKAGE_SLIME, 2: CEIL_TECH

#define SPR32_W             32
#define SPR32_H             32
#define SPR32_BYTES         ((SPR32_W * SPR32_H) / 2) // 512 bytes per 32x32 sprite

#define WEAPON32_W          64
#define WEAPON32_H          48
#define WEAPON32_BYTES      ((WEAPON32_W * WEAPON32_H) / 2) // 1536 bytes

#define FACE_W              24
#define FACE_H              20
#define FACE_BYTES          ((FACE_W * FACE_H) / 2) // 240 bytes

#define COLOR_4BPP_TRANS    0xF // Index 15 is color key / shadow

// Asset Storage Arrays in DMEM
extern uint8_t wall_textures[NUM_WALL_TEX][TEX64_BYTES];
extern uint8_t floor_flats[NUM_FLATS][FLAT32_BYTES];
extern uint8_t spr_imp_walk[2][SPR32_BYTES];
extern uint8_t spr_imp_attack[SPR32_BYTES];
extern uint8_t spr_imp_die[2][SPR32_BYTES];
extern uint8_t spr_fireball[SPR32_BYTES];
extern uint8_t spr_barrel[SPR32_BYTES];
extern uint8_t spr_medkit[SPR32_BYTES];
extern uint8_t weapon_shotgun[WEAPON32_BYTES];
extern uint8_t weapon_flash[WEAPON32_BYTES];
extern uint8_t doomguy_face_idle[FACE_BYTES];
extern uint8_t doomguy_face_grin[FACE_BYTES];
extern uint8_t doomguy_face_pain[FACE_BYTES];

// Initialize all 16-color textures, flats, sprites, and HUD assets
void init_doom_assets_4bpp(void);

// Configure the 16-entry 12-bit RGB palette in the VGA CLUT
void setup_doom_palette_4bpp(void);

#endif // DOOM_ASSETS_4BPP_H
