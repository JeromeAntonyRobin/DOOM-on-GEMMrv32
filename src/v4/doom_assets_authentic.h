#ifndef DOOM_ASSETS_AUTHENTIC_H
#define DOOM_ASSETS_AUTHENTIC_H

#include <stdint.h>
#include "doom_defs.h"

typedef struct {
    int16_t width;
    int16_t height;
    int16_t leftoffset;
    int16_t topoffset;
    const uint8_t *data;
} doom_patch_t;

extern const doom_patch_t patch_sht2a0;
extern const doom_patch_t patch_sposa1;
extern const doom_patch_t patch_trooa1;
extern const doom_patch_t patch_sarga1;
extern const doom_patch_t patch_stfst00;
extern const doom_patch_t patch_stfevl0;
extern const uint8_t tex_startan3[4096];

void setup_authentic_doom2_palette(void);
void draw_authentic_patch(volatile uint8_t *vram, const doom_patch_t *patch, int x, int y);
void draw_authentic_patch_scaled(volatile uint8_t *vram, const doom_patch_t *patch, int screenX, int screenY, int spr_w, int spr_h, fixed_t tz);

#endif // DOOM_ASSETS_AUTHENTIC_H
