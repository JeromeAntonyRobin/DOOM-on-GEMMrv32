#include "doom_assets.h"
#include <string.h>

uint8_t textures[NUM_TEXTURES][TEX_WIDTH * TEX_HEIGHT];
uint8_t sprites[NUM_SPRITES][SPRITE_W * SPRITE_H];
uint8_t weapon_shotgun[WEAPON_W * WEAPON_H];
uint8_t weapon_flash[WEAPON_W * WEAPON_H];
uint8_t doomguy_face[FACE_W * FACE_H];
uint8_t doomguy_grin[FACE_W * FACE_H];

// Palette index mapping constants:
// 10..19: Tech Blue shades
// 20..29: Slime Green shades
// 30..39: Red / Fire shades
// 40..49: Steel / Rust shades
// 60..69: Imp brown / tan shades
// 70..79: Toxic barrel green
// 80..89: Gunmetal grey
// 90..99: Fire / Muzzle flash yellow-orange
// 100..109: Face skin & hair

void init_doom_assets(void) {
    // -------------------------------------------------------------------------
    // 1. Texture 0: Tech Metal Panel (Wall Type 1)
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            uint8_t col = 13; // Base tech blue
            // Outer beveled border
            if (x == 0 || y == 0) col = 10; // Light highlight
            else if (x == 31 || y == 31) col = 16; // Dark shadow
            else if (y == 15 || y == 16) col = 17; // Horizontal middle seam
            // Corner bolts / rivets
            else if ((x == 3 || x == 28) && (y == 3 || y == 28 || y == 13 || y == 18)) {
                col = 52; // Bright bolt
            }
            // Vertical vent louvers in center
            else if (x >= 8 && x <= 23 && y >= 5 && y <= 11) {
                col = ((y % 2) == 0) ? 11 : 18;
            }
            else if (x >= 8 && x <= 23 && y >= 20 && y <= 26) {
                col = ((y % 2) == 0) ? 11 : 18;
            }
            textures[0][y * TEX_WIDTH + x] = col;
        }
    }

    // -------------------------------------------------------------------------
    // 2. Texture 1: Slime Brick / Stone Wall (Wall Type 2)
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            uint8_t col = 23; // Base slime stone
            // Horizontal mortar lines every 8 rows
            if ((y % 8) == 0) {
                col = 28; // Dark mortar
            } else {
                // Interlocking vertical mortar lines
                int brick_row = y / 8;
                int x_offset = (brick_row % 2 == 0) ? 0 : 16;
                if (((x + x_offset) % 16) == 0) {
                    col = 28; // Dark mortar
                } else if (((x + x_offset) % 16) == 1 || (y % 8) == 1) {
                    col = 21; // Brick highlight edge
                }
            }
            // Slime seepage drips down
            if (x >= 12 && x <= 14 && y >= 8 && y <= 24) {
                col = 20; // Glowing acidic green
            }
            textures[1][y * TEX_WIDTH + x] = col;
        }
    }

    // -------------------------------------------------------------------------
    // 3. Texture 2: Computer Console / Terminal (Wall Type 3)
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            uint8_t col = 35; // Dark chassis
            // Outer chassis frame
            if (x < 2 || x > 29 || y < 2 || y > 29) {
                col = 42; // Steel border
            }
            // CRT Terminal screen in top half
            else if (x >= 5 && x <= 26 && y >= 4 && y <= 16) {
                // CRT screen scanlines
                if (x == 5 || y == 4) col = 30; // Screen border
                else if ((y % 2) == 0) col = 32; // Cyan/Blue scanline
                else col = 36; // Dark scanline
                // Terminal text waveform / code
                if ((y == 7 || y == 10 || y == 13) && x >= 8 && x <= 22) {
                    if ((x % 3) != 0) col = 51; // Green phosphor text!
                }
            }
            // Keypad / switch matrix in bottom half
            else if (y >= 19 && y <= 27 && x >= 5 && x <= 26) {
                if (((x - 5) % 4 == 0) || ((y - 19) % 3 == 0)) {
                    col = 38; // Bezel
                } else {
                    // Blinking red/yellow status indicator LEDs
                    if (y <= 21 && x <= 10) col = 91; // Bright Red
                    else if (y <= 21 && x <= 16) col = 93; // Amber
                    else col = 82; // Keypad buttons
                }
            }
            textures[2][y * TEX_WIDTH + x] = col;
        }
    }

    // -------------------------------------------------------------------------
    // 4. Texture 3: Heavy Steel Plating & Rivets (Wall Type 4)
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            uint8_t col = 43; // Steel grey
            // Diamond tread / cross-hatch metal plate
            if (((x + y) % 6 == 0) || ((x - y + 32) % 6 == 0)) {
                col = 41; // Raised tread highlight
            } else if (((x + y) % 6 == 1) || ((x - y + 32) % 6 == 1)) {
                col = 46; // Tread shadow
            }
            // Steel panel border
            if (x == 0 || y == 0 || x == 31 || y == 31) {
                col = 47; // Dark border groove
            }
            // Corner heavy bolts
            if ((x == 3 || x == 28) && (y == 3 || y == 28)) {
                col = 52; // Chrome rivet
            }
            textures[3][y * TEX_WIDTH + x] = col;
        }
    }

    // -------------------------------------------------------------------------
    // 5. Sprite 0: Imp Demon (16x16 pixels)
    // -------------------------------------------------------------------------
    memset(sprites[0], COLOR_TRANSPARENT, SPRITE_W * SPRITE_H);
    for (int y = 0; y < SPRITE_H; y++) {
        for (int x = 0; x < SPRITE_W; x++) {
            // Head & Horns
            if (y >= 2 && y <= 6 && x >= 5 && x <= 10) {
                sprites[0][y * SPRITE_W + x] = 63; // Brown flesh
                // Horns
                if (y == 2 && (x == 5 || x == 10)) sprites[0][y * SPRITE_W + x] = 61; // Dark horn tip
                // Red glowing eyes
                if (y == 4 && (x == 6 || x == 9)) sprites[0][y * SPRITE_W + x] = 91; // Bright Red Eye
                // Fang / mouth
                if (y == 6 && (x == 7 || x == 8)) sprites[0][y * SPRITE_W + x] = 52; // White fang
            }
            // Muscular Torso & Spikes
            else if (y >= 7 && y <= 11 && x >= 4 && x <= 11) {
                sprites[0][y * SPRITE_W + x] = 62; // Tan muscle
                // Shoulder spikes
                if (y == 7 && (x == 3 || x == 12)) sprites[0][y * SPRITE_W + x] = 60;
                // Spine ridge
                if (x == 7 || x == 8) sprites[0][y * SPRITE_W + x] = 65; // Dark ridge
            }
            // Clawed Arms
            else if (y >= 8 && y <= 12 && (x == 2 || x == 3 || x == 12 || x == 13)) {
                sprites[0][y * SPRITE_W + x] = 64; // Dark claw
            }
            // Legs
            else if (y >= 12 && y <= 15 && ((x >= 4 && x <= 6) || (x >= 9 && x <= 11))) {
                sprites[0][y * SPRITE_W + x] = 63; // Leg
                if (y == 15) sprites[0][y * SPRITE_W + x] = 60; // Hoof / claw
            }
        }
    }

    // -------------------------------------------------------------------------
    // 6. Sprite 1: Explosive Toxic Barrel (16x16 pixels)
    // -------------------------------------------------------------------------
    memset(sprites[1], COLOR_TRANSPARENT, SPRITE_W * SPRITE_H);
    for (int y = 0; y < SPRITE_H; y++) {
        for (int x = 0; x < SPRITE_W; x++) {
            // Barrel cylindrical bounds
            if (y >= 2 && y <= 14 && x >= 3 && x <= 12) {
                uint8_t col = 73; // Toxic Green
                // Cylinder cylindrical lighting (left light, right dark)
                if (x == 3 || x == 4) col = 71; // Highlight
                else if (x == 11 || x == 12) col = 76; // Shadow
                // Metal reinforcement ribs
                if (y == 2 || y == 8 || y == 14) col = 42; // Steel rim
                // Nuclear / Biohazard symbol in center
                if (y >= 5 && y <= 7 && x >= 6 && x <= 9) {
                    col = 93; // Glowing Amber/Yellow Trefoil
                }
                // Slime leaking out on top
                if (y == 2 && (x == 6 || x == 7)) col = 20; // Bright slime green
                sprites[1][y * SPRITE_W + x] = col;
            }
        }
    }

    // -------------------------------------------------------------------------
    // 7. Sprite 2: Health Medkit (16x16 pixels)
    // -------------------------------------------------------------------------
    memset(sprites[2], COLOR_TRANSPARENT, SPRITE_W * SPRITE_H);
    for (int y = 0; y < SPRITE_H; y++) {
        for (int x = 0; x < SPRITE_W; x++) {
            // Rectangular crate body
            if (y >= 5 && y <= 13 && x >= 2 && x <= 13) {
                uint8_t col = 52; // Medical White
                // Top & side bevels
                if (y == 5) col = 50; // Bright white
                else if (y == 13 || x == 13) col = 84; // Grey shadow
                else if (x == 2) col = 82;
                // Red Cross icon in center
                if ((y >= 7 && y <= 11 && x >= 7 && x <= 8) || // Vertical bar
                    (y >= 8 && y <= 10 && x >= 5 && x <= 10)) { // Horizontal bar
                    col = 91; // Bright Red Cross
                }
                sprites[2][y * SPRITE_W + x] = col;
            }
        }
    }

    // -------------------------------------------------------------------------
    // 8. First-Person Shotgun Weapon Sprite (32x24 pixels)
    // -------------------------------------------------------------------------
    memset(weapon_shotgun, COLOR_TRANSPARENT, WEAPON_W * WEAPON_H);
    for (int y = 0; y < WEAPON_H; y++) {
        for (int x = 0; x < WEAPON_W; x++) {
            // Twin Gun Barrels (Top half: y = 0..11)
            if (y >= 0 && y <= 11) {
                // Left Barrel: x = 12..14
                if (x >= 12 && x <= 14) {
                    uint8_t col = (x == 12) ? 81 : (x == 13 ? 83 : 86);
                    if (y == 0) col = 88; // Muzzle opening bore
                    weapon_shotgun[y * WEAPON_W + x] = col;
                }
                // Right Barrel: x = 17..19
                else if (x >= 17 && x <= 19) {
                    uint8_t col = (x == 17) ? 81 : (x == 18 ? 83 : 86);
                    if (y == 0) col = 88; // Muzzle opening bore
                    weapon_shotgun[y * WEAPON_W + x] = col;
                }
                // Center rib between barrels
                else if (x >= 15 && x <= 16 && y >= 2) {
                    weapon_shotgun[y * WEAPON_W + x] = 85;
                }
            }
            // Wooden Forend / Grip (y = 12..19, x = 11..20)
            else if (y >= 12 && y <= 19 && x >= 11 && x <= 20) {
                uint8_t col = (x <= 13) ? 61 : ((x >= 18) ? 64 : 62); // Walnut wood
                if ((y % 3) == 0) col = 65; // Wood grain
                weapon_shotgun[y * WEAPON_W + x] = col;
            }
            // Doomguy gloved hands holding gun (y = 18..23, x = 8..23)
            else if (y >= 18 && y <= 23 && x >= 8 && x <= 23) {
                if (x <= 13 || x >= 18) {
                    weapon_shotgun[y * WEAPON_W + x] = 45; // Brown leather glove
                    if (x == 11 || x == 20) weapon_shotgun[y * WEAPON_W + x] = 42; // Glove knuckle
                }
            }
        }
    }

    // Shotgun Muzzle Flash Animation Frame
    memcpy(weapon_flash, weapon_shotgun, WEAPON_W * WEAPON_H);
    for (int y = 0; y < 10; y++) {
        for (int x = 8; x <= 23; x++) {
            // Fiery starburst muzzle blast
            int dx = x - 15;
            int dy = y - 4;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 16) {
                weapon_flash[y * WEAPON_W + x] = 90; // Pure bright yellow core
            } else if (dist_sq <= 36) {
                weapon_flash[y * WEAPON_W + x] = 92; // Intense orange blast
            } else if (dist_sq <= 55 && ((x + y) % 2 == 0)) {
                weapon_flash[y * WEAPON_W + x] = 91; // Red fire spark
            }
        }
    }

    // -------------------------------------------------------------------------
    // 9. Doomguy Status Bar Face (16x12 pixels)
    // -------------------------------------------------------------------------
    memset(doomguy_face, 110, FACE_W * FACE_H); // Grey background
    for (int y = 0; y < FACE_H; y++) {
        for (int x = 0; x < FACE_W; x++) {
            // Hair (y = 0..2)
            if (y <= 2 && x >= 3 && x <= 12) {
                doomguy_face[y * FACE_W + x] = 65; // Dark brown military crew cut
            }
            // Face skin (y = 3..9, x = 3..12)
            else if (y >= 3 && y <= 9 && x >= 3 && x <= 12) {
                uint8_t col = 101; // Flesh tone
                // Cheek shadows
                if (x == 3 || x == 12) col = 103;
                // Eyes (y = 4, x = 5, 10)
                if (y == 4 && (x == 5 || x == 10)) col = 50; // White eye
                if (y == 4 && (x == 6 || x == 9))  col = 14; // Blue pupil
                // Furrowed eyebrows
                if (y == 3 && ((x >= 4 && x <= 6) || (x >= 9 && x <= 11))) col = 65;
                // Nose
                if (y >= 5 && y <= 6 && (x == 7 || x == 8)) col = 102;
                // Mouth / Grimace
                if (y == 8 && x >= 6 && x <= 9) col = 104; // Determined grimace
                // Strong jaw / chin
                if (y == 9 && x >= 5 && x <= 10) col = 102;
                doomguy_face[y * FACE_W + x] = col;
            }
            // Neck / Armor collar (y = 10..11)
            else if (y >= 10 && x >= 4 && x <= 11) {
                doomguy_face[y * FACE_W + x] = 22; // Green combat armor
            }
        }
    }

    // Doomguy Evil Grin (when weapon fires!)
    memcpy(doomguy_grin, doomguy_face, FACE_W * FACE_H);
    for (int x = 5; x <= 10; x++) {
        doomguy_grin[8 * FACE_W + x] = 50; // Tooth grin!
        doomguy_grin[7 * FACE_W + x] = 104;
    }
}
