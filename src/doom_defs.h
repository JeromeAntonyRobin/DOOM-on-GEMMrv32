// ==============================================================================
// GEMMrv32 DOOM v3: Authentic Id Software Data Structures (Native 320x200 @ 4bpp)
// Adapted for bare-metal RISC-V 32-bit execution & 8x8 Systolic Coprocessor
// ==============================================================================

#ifndef DOOM_DEFS_H
#define DOOM_DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Screen Dimensions (Authentic Mode 13h 320x200 Framebuffer @ 8bpp: 64,000 bytes)
#define SCREEN_W            320
#define SCREEN_H            200
#define VRAM_BYTES          (SCREEN_W * SCREEN_H) // 64,000 bytes at 8bpp (256 colors)

#define VIEWPORT_H         168 // Top 168 scanlines (0..167) for 3D world
#define STATUS_BAR_H        32  // Bottom 32 scanlines (168..199) for HUD
#define CENTER_X           (SCREEN_W / 2)   // 160
#define CENTER_Y           (VIEWPORT_H / 2) // 84
#define FOCAL_LEN          160              // 160 units for authentic 90-deg FOV across 320 pixels

// 16.16 Fixed-Point Arithmetic Definitions
typedef int32_t fixed_t;
#define FP_FRACBITS         16
#define FP_ONE              (1 << FP_FRACBITS)
#define TO_FIXED(x)         ((fixed_t)((x) * FP_ONE))
#define FROM_FIXED(x)       ((int32_t)((x) >> FP_FRACBITS))
#define FIXED_MUL(a, b)     ((fixed_t)(((int64_t)(a) * (int64_t)(b)) >> FP_FRACBITS))
#define FIXED_DIV(a, b)     ((fixed_t)((((a) << 8) / ((b) >> 8))))

typedef uint32_t angle_t;
#define ANG45               0x20000000
#define ANG90               0x40000000
#define ANG180              0x80000000
#define ANG270              0xC0000000

// ------------------------------------------------------------------------------
// 1. World Geometry Structures (Linedefs, Sidedefs, Sectors, Vertices)
// ------------------------------------------------------------------------------

typedef struct {
    fixed_t x;
    fixed_t y;
} vertex_t;

typedef struct {
    fixed_t   floorheight;
    fixed_t   ceilingheight;
    int16_t   floorpic;
    int16_t   ceilingpic;
    int16_t   lightlevel;
    int16_t   validcount;
} sector_t;

typedef struct {
    fixed_t   textureoffset;
    fixed_t   rowoffset;
    int16_t   toptexture;
    int16_t   bottomtexture;
    int16_t   midtexture;
    sector_t  *sector;
} side_t;

#define ML_BLOCKING         1
#define ML_TWOSIDED         4
#define ML_DONTPEGTOP       8
#define ML_DONTPEGBOTTOM    16

typedef struct {
    vertex_t  *v1;
    vertex_t  *v2;
    fixed_t   dx;
    fixed_t   dy;
    int16_t   flags;
    sector_t  *frontsector;
    sector_t  *backsector;
} line_t;

// ------------------------------------------------------------------------------
// 2. 2D Binary Space Partitioning (BSP) Structures
// ------------------------------------------------------------------------------

#define NF_SUBSECTOR        0x8000

typedef struct {
    fixed_t   x;
    fixed_t   y;
    fixed_t   dx;
    fixed_t   dy;
    fixed_t   bbox[2][4];  // [0:Front, 1:Back][0:Top, 1:Bottom, 2:Left, 3:Right]
    uint16_t  children[2]; // Child node index or (Subsector index | NF_SUBSECTOR)
} node_t;

typedef struct {
    sector_t  *sector;
    int16_t   numlines;
    int16_t   firstline;
} subsector_t;

typedef struct {
    vertex_t  *v1;
    vertex_t  *v2;
    fixed_t   offset;
    angle_t   angle;
    side_t    *sidedef;
    line_t    *linedef;
    sector_t  *frontsector;
    sector_t  *backsector;
} seg_t;

// ------------------------------------------------------------------------------
// 3. Occlusion & Visplane Structures
// ------------------------------------------------------------------------------

typedef struct {
    int16_t   first;
    int16_t   last;
} cliprange_t;

#define MAX_SOLIDSEGS       64
extern cliprange_t solidsegs[MAX_SOLIDSEGS];
extern int num_solidsegs;

typedef struct {
    fixed_t   height;
    int16_t   picnum;
    int16_t   light;
    int16_t   minx;
    int16_t   maxx;
    uint8_t   top[SCREEN_W];
    uint8_t   bottom[SCREEN_W];
} visplane_t;

#define MAX_VISPLANES       32
extern visplane_t visplanes[MAX_VISPLANES];
extern int num_visplanes;

// ------------------------------------------------------------------------------
// 4. Map Object (Mobj) Finite-State Machine Definitions
// ------------------------------------------------------------------------------

typedef enum {
    MT_PLAYER,
    MT_IMP,
    MT_SHOTGUNGUY,
    MT_DEMON,
    MT_ZOMBIEMAN,
    MT_BARREL,
    MT_FIREBALL,
    MT_MEDKIT,
    MT_SHOTGUNBOX
} mobjtype_t;

typedef enum {
    // Imp States
    S_IMP_SPAWN,
    S_IMP_SEE1, S_IMP_SEE2, S_IMP_SEE3, S_IMP_SEE4,
    S_IMP_ATK1, S_IMP_ATK2,
    S_IMP_PAIN,
    S_IMP_DIE1, S_IMP_DIE2, S_IMP_DIE3, S_IMP_DIE4, S_IMP_DEAD,
    // Projectiles
    S_FIREBALL1, S_FIREBALL2, S_FIREBALL_BURST,
    // Barrels
    S_BARREL_IDLE, S_BARREL_EXPLODE1, S_BARREL_EXPLODE2, S_BARREL_DEAD
} statenum_t;

typedef struct mobj_s {
    fixed_t   x, y, z;
    fixed_t   momx, momy, momz; // Momentum
    angle_t   angle;
    struct mobj_s *target;      // Target entity (Player)
    int16_t   health;
    int16_t   tics;             // Tics remaining in current animation frame
    statenum_t state;
    mobjtype_t type;
    fixed_t   radius;
    fixed_t   height;
    int16_t   flags;
} mobj_t;

#define MF_SOLID            1
#define MF_SHOOTABLE        2
#define MF_NOBLOCKMAP       4
#define MF_MISSILE          8
#define MF_COUNTKILL        16

#define MAX_MOBJS           32
extern mobj_t mobjs[MAX_MOBJS];
extern int num_mobjs;

// ------------------------------------------------------------------------------
// 5. Native 8bpp Framebuffer Manipulation Macros (Mode 13h: 1 Byte = 1 Pixel)
// ------------------------------------------------------------------------------

#define VGA_VRAM_BASE       0x90000000
#define VGA_PALETTE_BASE    0x90010000

static inline void put_pixel(volatile uint8_t *vram, int x, int y, uint8_t col) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        vram[y * SCREEN_W + x] = col;
    }
}

static inline uint8_t get_pixel(const volatile uint8_t *vram, int x, int y) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H) {
        return vram[y * SCREEN_W + x];
    }
    return 0;
}

#define VGA_STATUS_REG      0x90010400
#define VGA_STAT_VBLANK     (1 << 0)

static inline void wait_for_vblank(void) {
    // Hardware VBlank status register 0x90010400 returns 0 on xc7a35t single-buffered BRAM.
    // Stubbed to no-op to eliminate 110ms artificial delay per frame.
}

#endif // DOOM_DEFS_H
