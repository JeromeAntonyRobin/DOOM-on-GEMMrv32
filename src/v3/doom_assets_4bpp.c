#include "doom_assets_4bpp.h"
#include <string.h>

// Asset Storage in DMEM
uint8_t wall_textures[NUM_WALL_TEX][TEX64_BYTES];
uint8_t floor_flats[NUM_FLATS][FLAT32_BYTES];
uint8_t spr_imp_walk[2][SPR32_BYTES];
uint8_t spr_imp_attack[SPR32_BYTES];
uint8_t spr_imp_die[2][SPR32_BYTES];
uint8_t spr_fireball[SPR32_BYTES];
uint8_t spr_barrel[SPR32_BYTES];
uint8_t spr_medkit[SPR32_BYTES];
uint8_t weapon_shotgun[WEAPON32_BYTES];
uint8_t weapon_flash[WEAPON32_BYTES];
uint8_t doomguy_face_idle[FACE_BYTES];
uint8_t doomguy_face_grin[FACE_BYTES];
uint8_t doomguy_face_pain[FACE_BYTES];

#define VGA_PALETTE_BASE 0x90010000

// Helper to write a 4bpp nibble pixel into an asset buffer
static inline void set_asset_pix(uint8_t *buf, int w, int x, int y, uint8_t col) {
    int byte_idx = (y * w + x) >> 1;
    if (x & 1) {
        buf[byte_idx] = (buf[byte_idx] & 0xF0) | (col & 0x0F);
    } else {
        buf[byte_idx] = (buf[byte_idx] & 0x0F) | ((col & 0x0F) << 4);
    }
}

// ------------------------------------------------------------------------------
// Setup 16-Color 12-bit RGB Palette
// ------------------------------------------------------------------------------
void setup_doom_palette_4bpp(void) {
    volatile uint32_t *clut = (volatile uint32_t *)VGA_PALETTE_BASE;

    static const uint16_t doom_colors[16] = {
        0x000, // 0: Void Black
        0x222, // 1: Dark Grey
        0x555, // 2: Medium Grey
        0xAAA, // 3: Light Grey
        0xFFF, // 4: Pure White
        0xB00, // 5: Blood Red
        0xF60, // 6: Fire Orange
        0xFA0, // 7: Gold Amber
        0xFF0, // 8: Bright Yellow
        0x0B0, // 9: Acid Slime Green
        0x050, // 10: Dark Slime Green
        0x03B, // 11: Tech Blue
        0x08F, // 12: Light Blue
        0x731, // 13: Rust Brown
        0xC85, // 14: Flesh Tan
        0x111  // 15: Deep Shadow / Color Key
    };

    for (int i = 0; i < 16; i++) {
        clut[i] = doom_colors[i];
    }
}

// ------------------------------------------------------------------------------
// Initialize 4bpp Textures, Flats, Sprites & HUD Assets
// ------------------------------------------------------------------------------
void init_doom_assets_4bpp(void) {
    // -------------------------------------------------------------------------
    // 1. Texture 0: STARTAN3 (Tech Starport Tan/Steel Wall) - 64x64
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX64_H; y++) {
        for (int x = 0; x < TEX64_W; x++) {
            uint8_t col = 14; // Base tan panel
            // Vertical structural steel beams at x = 0..3, 30..33, 60..63
            if (x <= 3 || (x >= 30 && x <= 33) || x >= 60) {
                col = (x == 0 || x == 30 || x == 60) ? 3 : 1; // Highlight / shadow beam
            }
            // Horizontal steel braces at y = 0..3, 60..63, and middle y = 29..32
            else if (y <= 3 || y >= 60 || (y >= 29 && y <= 32)) {
                col = (y == 0 || y == 29) ? 3 : 1;
            }
            // Corner heavy hex-bolts
            else if ((x == 6 || x == 27 || x == 36 || x == 57) &&
                     (y == 6 || y == 26 || y == 35 || y == 56)) {
                col = 4; // Chrome bolt
            }
            // Recessed tech ventilation louvers in center of panels
            else if ((x >= 8 && x <= 25) || (x >= 38 && x <= 55)) {
                if ((y >= 10 && y <= 22) || (y >= 39 && y <= 51)) {
                    col = ((y % 2) == 0) ? 2 : 15; // Alternating metal slits
                }
            }
            set_asset_pix(wall_textures[0], TEX64_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 2. Texture 1: BROWN96 (Classic Brown Stone Masonry Wall) - 64x64
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX64_H; y++) {
        for (int x = 0; x < TEX64_W; x++) {
            uint8_t col = 13; // Rust brown stone
            // Horizontal mortar joint every 16 scanlines
            if ((y % 16) == 0) {
                col = 15; // Dark mortar
            } else {
                int brick_row = y / 16;
                int x_offset = (brick_row % 2 == 0) ? 0 : 32;
                // Interlocking vertical mortar joints every 32 texels
                if (((x + x_offset) % 32) == 0) {
                    col = 15;
                } else if (((x + x_offset) % 32) == 1 || (y % 16) == 1) {
                    col = 14; // Upper-left stone highlight edge
                } else if (((x + x_offset) % 32) == 31 || (y % 16) == 15) {
                    col = 1;  // Lower-right stone bevel shadow
                }
            }
            // Green toxic moss / slime seeping down cracks
            if (x >= 20 && x <= 23 && y >= 16 && y <= 48) {
                col = 9; // Acid slime green
            }
            set_asset_pix(wall_textures[1], TEX64_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 3. Texture 2: COMP01 (Computer Terminal Console Wall) - 64x64
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX64_H; y++) {
        for (int x = 0; x < TEX64_W; x++) {
            uint8_t col = 2; // Dark chassis grey
            // Outer chassis frame
            if (x <= 3 || x >= 60 || y <= 3 || y >= 60) {
                col = (x <= 1 || y <= 1) ? 3 : 1;
            }
            // Large CRT Terminal Screen in upper half (x = 8..55, y = 8..34)
            else if (x >= 8 && x <= 55 && y >= 8 && y <= 34) {
                if (x == 8 || y == 8) col = 1; // Screen bevel
                else if (x == 55 || y == 34) col = 3;
                else if ((y % 2) == 0) col = 11; // Tech blue scanlines
                else col = 0; // Black scanlines
                // Phosphor text / waveform data
                if ((y == 14 || y == 18 || y == 22 || y == 26) && (x >= 12 && x <= 50)) {
                    if ((x % 4) != 0) col = 9; // Glowing green code!
                }
            }
            // Lower half: Keypad buttons & status LED arrays
            else if (y >= 40 && y <= 56 && x >= 8 && x <= 55) {
                if (((x - 8) % 6 == 0) || ((y - 40) % 5 == 0)) {
                    col = 1; // Grid divider
                } else {
                    // Blinking colored LEDs & switches
                    if (x <= 20) col = 5; // Red warning LEDs
                    else if (x <= 32) col = 7; // Amber caution LEDs
                    else if (x <= 44) col = 9; // Green normal LEDs
                    else col = 3; // Keypad buttons
                }
            }
            set_asset_pix(wall_textures[2], TEX64_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 4. Texture 3: DOOR3 (Heavy Hazard Blast Door) - 64x64
    // -------------------------------------------------------------------------
    for (int y = 0; y < TEX64_H; y++) {
        for (int x = 0; x < TEX64_W; x++) {
            uint8_t col = 2; // Steel grey
            // Outer doorframe border
            if (x <= 4 || x >= 59 || y <= 4 || y >= 59) {
                col = (x <= 2 || y <= 2) ? 3 : 1;
            }
            // Yellow and black diagonal warning hazard stripes at top (y = 6..14)
            else if (y >= 6 && y <= 14) {
                col = (((x + y) / 4) % 2 == 0) ? 8 : 0; // Yellow / Black stripes
            }
            // Central hydraulic door seam (vertical slit at x = 31..32)
            else if (x == 31 || x == 32) {
                col = 0; // Deep shadow seal
            }
            // Door lock panel / keycard scanner in center (x = 26..37, y = 32..44)
            else if (x >= 26 && x <= 37 && y >= 32 && y <= 44) {
                col = (y == 32 || x == 26) ? 3 : 1;
                if (x >= 29 && x <= 34 && y >= 35 && y <= 41) {
                    col = 5; // Glowing red access denied light
                }
            }
            // Recessed horizontal armor plates
            else if ((y % 12) == 0) {
                col = 1; // Plate groove
            }
            set_asset_pix(wall_textures[3], TEX64_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 5. Floor Flats (32x32)
    // -------------------------------------------------------------------------
    // Flat 0: FLOOR_STONE (Diamond tread metal plate)
    for (int y = 0; y < FLAT32_H; y++) {
        for (int x = 0; x < FLAT32_W; x++) {
            uint8_t col = 1; // Dark grey
            if (((x + y) % 6 == 0) || ((x - y + 32) % 6 == 0)) col = 3; // Light diamond tread
            else if (((x + y) % 6 == 1) || ((x - y + 32) % 6 == 1)) col = 0; // Shadow
            set_asset_pix(floor_flats[0], FLAT32_W, x, y, col);
        }
    }

    // Flat 1: NUKAGE_SLIME (Acid green toxic waste pool)
    for (int y = 0; y < FLAT32_H; y++) {
        for (int x = 0; x < FLAT32_W; x++) {
            uint8_t col = 10; // Dark slime
            if (((x * 7 + y * 13) % 11) < 4) col = 9; // Bright acidic swirl
            // Slime bubble highlights
            if ((x == 8 && y == 8) || (x == 22 && y == 19) || (x == 14 && y == 27)) col = 4;
            set_asset_pix(floor_flats[1], FLAT32_W, x, y, col);
        }
    }

    // Flat 2: CEIL_TECH (Square blue tech ceiling tiles)
    for (int y = 0; y < FLAT32_H; y++) {
        for (int x = 0; x < FLAT32_W; x++) {
            uint8_t col = 11; // Tech blue
            if (x == 0 || y == 0 || x == 31 || y == 31) col = 1; // Tile groove
            else if (x >= 12 && x <= 19 && y >= 4 && y <= 27) col = 4; // Recessed fluorescent lamp
            set_asset_pix(floor_flats[2], FLAT32_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 6. Sprites (32x32) - Imp Demon (Walk 1 & 2, Attack, Die)
    // -------------------------------------------------------------------------
    memset(spr_imp_walk[0], COLOR_4BPP_TRANS, SPR32_BYTES);
    memset(spr_imp_walk[1], COLOR_4BPP_TRANS, SPR32_BYTES);
    memset(spr_imp_attack,  COLOR_4BPP_TRANS, SPR32_BYTES);
    memset(spr_imp_die[0],  COLOR_4BPP_TRANS, SPR32_BYTES);
    memset(spr_imp_die[1],  COLOR_4BPP_TRANS, SPR32_BYTES);

    // Imp Walk Frame 1
    for (int y = 4; y <= 28; y++) {
        for (int x = 8; x <= 23; x++) {
            // Head & Horns (y = 4..10, x = 11..20)
            if (y >= 4 && y <= 10 && x >= 11 && x <= 20) {
                uint8_t col = 14; // Tan skin
                if (y <= 5 && (x == 11 || x == 20)) col = 13; // Brown horns
                if (y == 7 && (x == 13 || x == 18)) col = 5;  // Glowing red eyes
                if (y == 9 && (x == 15 || x == 16)) col = 4;  // White fangs
                set_asset_pix(spr_imp_walk[0], SPR32_W, x, y, col);
            }
            // Muscular Torso & Spikes (y = 11..20, x = 9..22)
            else if (y >= 11 && y <= 20 && x >= 9 && x <= 22) {
                uint8_t col = 14;
                if (y == 11 && (x == 8 || x == 23)) col = 13; // Shoulder spikes
                if (x == 15 || x == 16) col = 13; // Spine ridge
                set_asset_pix(spr_imp_walk[0], SPR32_W, x, y, col);
            }
            // Legs (y = 21..28)
            else if (y >= 21 && y <= 28 && ((x >= 10 && x <= 13) || (x >= 18 && x <= 21))) {
                set_asset_pix(spr_imp_walk[0], SPR32_W, x, y, 13);
            }
        }
    }

    // Imp Walk Frame 2 (Alternating stride)
    memcpy(spr_imp_walk[1], spr_imp_walk[0], SPR32_BYTES);

    // Imp Attack Frame (Arms raised to throw fireball)
    memcpy(spr_imp_attack, spr_imp_walk[0], SPR32_BYTES);
    for (int y = 6; y <= 12; y++) {
        for (int x = 4; x <= 8; x++)   set_asset_pix(spr_imp_attack, SPR32_W, x, y, 14); // Left raised arm
        for (int x = 23; x <= 27; x++) set_asset_pix(spr_imp_attack, SPR32_W, x, y, 14); // Right raised arm
    }

    // Imp Death Frame (Collapsing)
    for (int y = 16; y <= 30; y++) {
        for (int x = 6; x <= 26; x++) {
            uint8_t col = 13;
            if (y >= 24) col = 5; // Pool of blood
            set_asset_pix(spr_imp_die[0], SPR32_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 7. Fireball Projectile Sprite (32x32)
    // -------------------------------------------------------------------------
    memset(spr_fireball, COLOR_4BPP_TRANS, SPR32_BYTES);
    for (int y = 8; y <= 23; y++) {
        for (int x = 8; x <= 23; x++) {
            int dx = x - 15;
            int dy = y - 15;
            int dsq = dx * dx + dy * dy;
            if (dsq <= 9)       set_asset_pix(spr_fireball, SPR32_W, x, y, 8); // White-hot yellow core
            else if (dsq <= 25) set_asset_pix(spr_fireball, SPR32_W, x, y, 6); // Fire orange
            else if (dsq <= 49) set_asset_pix(spr_fireball, SPR32_W, x, y, 5); // Red halo
        }
    }

    // -------------------------------------------------------------------------
    // 8. Toxic Barrel Sprite (32x32)
    // -------------------------------------------------------------------------
    memset(spr_barrel, COLOR_4BPP_TRANS, SPR32_BYTES);
    for (int y = 4; y <= 28; y++) {
        for (int x = 8; x <= 23; x++) {
            uint8_t col = 9; // Toxic slime green
            if (x == 8 || x == 9) col = 4; // Highlight
            else if (x == 22 || x == 23) col = 10; // Shadow
            if (y == 4 || y == 16 || y == 28) col = 2; // Metal hoops
            if (y >= 10 && y <= 14 && x >= 13 && x <= 18) col = 7; // Yellow biohazard trefoil
            set_asset_pix(spr_barrel, SPR32_W, x, y, col);
        }
    }

    // -------------------------------------------------------------------------
    // 9. Shotgun First-Person Weapon Sprite (64x48)
    // -------------------------------------------------------------------------
    memset(weapon_shotgun, COLOR_4BPP_TRANS, WEAPON32_BYTES);
    for (int y = 0; y < WEAPON32_H; y++) {
        for (int x = 0; x < WEAPON32_W; x++) {
            // Twin Gun Barrels (x = 24..39, y = 0..24)
            if (y <= 24 && x >= 24 && x <= 39) {
                // Left barrel: x = 25..30
                if (x >= 25 && x <= 30) {
                    uint8_t col = (x == 25) ? 3 : (x == 26 ? 2 : 1);
                    if (y == 0) col = 0; // Bore
                    set_asset_pix(weapon_shotgun, WEAPON32_W, x, y, col);
                }
                // Right barrel: x = 33..38
                else if (x >= 33 && x <= 38) {
                    uint8_t col = (x == 33) ? 3 : (x == 34 ? 2 : 1);
                    if (y == 0) col = 0; // Bore
                    set_asset_pix(weapon_shotgun, WEAPON32_W, x, y, col);
                }
            }
            // Walnut wooden forend grip (x = 22..41, y = 25..38)
            else if (y >= 25 && y <= 38 && x >= 22 && x <= 41) {
                uint8_t col = (x <= 26) ? 14 : ((x >= 37) ? 13 : 13);
                set_asset_pix(weapon_shotgun, WEAPON32_W, x, y, col);
            }
            // Doomguy leather combat gloves (x = 16..47, y = 35..47)
            else if (y >= 35 && y <= 47 && x >= 16 && x <= 47) {
                if (x <= 26 || x >= 37) {
                    set_asset_pix(weapon_shotgun, WEAPON32_W, x, y, 13);
                }
            }
        }
    }

    // Shotgun Muzzle Blast Sprite
    memcpy(weapon_flash, weapon_shotgun, WEAPON32_BYTES);
    for (int y = 0; y <= 18; y++) {
        for (int x = 16; x <= 47; x++) {
            int dx = x - 31;
            int dy = y - 8;
            int dsq = dx * dx + dy * dy;
            if (dsq <= 36)       set_asset_pix(weapon_flash, WEAPON32_W, x, y, 8); // White-yellow core
            else if (dsq <= 100) set_asset_pix(weapon_flash, WEAPON32_W, x, y, 6); // Fire orange
            else if (dsq <= 160 && ((x + y) % 2 == 0)) set_asset_pix(weapon_flash, WEAPON32_W, x, y, 5);
        }
    }

    // -------------------------------------------------------------------------
    // 10. Doomguy Status Bar Face (24x20)
    // -------------------------------------------------------------------------
    memset(doomguy_face_idle, 2, FACE_BYTES); // Grey status bar background
    for (int y = 0; y < FACE_H; y++) {
        for (int x = 0; x < FACE_W; x++) {
            // Military Haircut (y = 1..4, x = 4..19)
            if (y >= 1 && y <= 4 && x >= 4 && x <= 19) {
                set_asset_pix(doomguy_face_idle, FACE_W, x, y, 13); // Brown hair
            }
            // Face Skin (y = 5..15, x = 4..19)
            else if (y >= 5 && y <= 15 && x >= 4 && x <= 19) {
                uint8_t col = 14; // Flesh skin
                if (x == 4 || x == 19) col = 13; // Jaw shadow
                // Eyes (y = 7, x = 7..8, 15..16)
                if (y == 7 && (x == 7 || x == 15)) col = 4;  // Eye white
                if (y == 7 && (x == 8 || x == 16)) col = 12; // Blue pupil
                // Eyebrows
                if (y == 6 && ((x >= 6 && x <= 9) || (x >= 14 && x <= 17))) col = 13;
                // Mouth / Grimace (y = 13, x = 9..14)
                if (y == 13 && x >= 9 && x <= 14) col = 0;
                set_asset_pix(doomguy_face_idle, FACE_W, x, y, col);
            }
            // Green Combat Armor Collar (y = 16..19, x = 3..20)
            else if (y >= 16 && x >= 3 && x <= 20) {
                set_asset_pix(doomguy_face_idle, FACE_W, x, y, 10); // Green armor
            }
        }
    }

    // Doomguy Grin (Firing)
    memcpy(doomguy_face_grin, doomguy_face_idle, FACE_BYTES);
    for (int x = 8; x <= 15; x++) {
        set_asset_pix(doomguy_face_grin, FACE_W, x, 13, 4); // White teeth grin!
    }

    // Doomguy Pain (Damage)
    memcpy(doomguy_face_pain, doomguy_face_idle, FACE_BYTES);
    for (int x = 8; x <= 15; x++) {
        set_asset_pix(doomguy_face_pain, FACE_W, x, 13, 5); // Bloodied grimace
    }
}
