#ifndef DOOM_ASSETS_H
#define DOOM_ASSETS_H

#include <stdint.h>

#define TEX_WIDTH   32
#define TEX_HEIGHT  32
#define NUM_TEXTURES 4

#define SPRITE_W    16
#define SPRITE_H    16
#define NUM_SPRITES  3 // 0: Imp, 1: Barrel, 2: Medkit

#define COLOR_TRANSPARENT 0xFF

// 4 Textures of 32x32 pixels (4096 bytes total)
extern uint8_t textures[NUM_TEXTURES][TEX_WIDTH * TEX_HEIGHT];

// 3 Sprites of 16x16 pixels (768 bytes total)
extern uint8_t sprites[NUM_SPRITES][SPRITE_W * SPRITE_H];

// Shotgun Weapon Sprite (32x24 pixels, 768 bytes)
#define WEAPON_W 32
#define WEAPON_H 24
extern uint8_t weapon_shotgun[WEAPON_W * WEAPON_H];
extern uint8_t weapon_flash[WEAPON_W * WEAPON_H];

// Doomguy Face (16x12 pixels, 192 bytes)
#define FACE_W 16
#define FACE_H 12
extern uint8_t doomguy_face[FACE_W * FACE_H];
extern uint8_t doomguy_grin[FACE_W * FACE_H];

// Initialize all textures and sprite graphics into RAM
void init_doom_assets(void);

#endif // DOOM_ASSETS_H
