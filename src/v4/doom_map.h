// ==============================================================================
// GEMMrv32 DOOM v3: E1M1 Hangar & Toxic Courtyard Geometry (BSP & Linedefs)
// Fixed-Point 16.16 Coordinates with Variable Heights (Stairs, Slime Pit, Portals)
// ==============================================================================

#ifndef DOOM_MAP_H
#define DOOM_MAP_H

#include "doom_defs.h"

// ------------------------------------------------------------------------------
// 1. Map Vertices (16.16 Fixed-Point Coordinates)
// ------------------------------------------------------------------------------
static const vertex_t map_vertices[] __attribute__((unused)) = {
    // Sector 0: Starting Hangar (0..3)
    { TO_FIXED(-200), TO_FIXED(-200) }, // 0: SW
    { TO_FIXED( 200), TO_FIXED(-200) }, // 1: SE
    { TO_FIXED( 200), TO_FIXED( 100) }, // 2: NE / Comp NW
    { TO_FIXED(-200), TO_FIXED( 100) }, // 3: NW

    // Sector 1: Stairway Portal to Courtyard (4..7)
    { TO_FIXED(-100), TO_FIXED( 100) }, // 4: Stair SW / Hangar N1
    { TO_FIXED( 100), TO_FIXED( 100) }, // 5: Stair SE / Hangar N2
    { TO_FIXED( 100), TO_FIXED( 250) }, // 6: Stair NE / Courtyard S2
    { TO_FIXED(-100), TO_FIXED( 250) }, // 7: Stair NW / Courtyard S1

    // Sector 2: Sunken Toxic Slime Courtyard (8..11)
    { TO_FIXED(-350), TO_FIXED( 250) }, // 8: Courtyard SW
    { TO_FIXED( 350), TO_FIXED( 250) }, // 9: Courtyard SE
    { TO_FIXED( 350), TO_FIXED( 600) }, // 10: Courtyard NE
    { TO_FIXED(-350), TO_FIXED( 600) }, // 11: Courtyard NW

    // Sector 3: Computer Terminal Control Room (12..14)
    { TO_FIXED( 200), TO_FIXED(-100) }, // 12: Comp SW / Hangar Portal South
    { TO_FIXED( 400), TO_FIXED(-100) }, // 13: Comp SE
    { TO_FIXED( 400), TO_FIXED( 100) }  // 14: Comp NE
};

#define NUM_VERTICES (sizeof(map_vertices) / sizeof(map_vertices[0]))

// ------------------------------------------------------------------------------
// 2. Sectors (Elevations, Flats, Light Levels)
// ------------------------------------------------------------------------------
static sector_t map_sectors[] __attribute__((unused)) = {
    // Sector 0: Starting Starport Hangar
    {
        .floorheight   = TO_FIXED(0),
        .ceilingheight = TO_FIXED(128),
        .floorpic      = 0, // Stone tread plate
        .ceilingpic    = 2, // Tech blue ceiling
        .lightlevel    = 180,
        .validcount    = 0
    },
    // Sector 1: Elevated Staircase Steps (Stairs step up to floor +32!)
    {
        .floorheight   = TO_FIXED(32),
        .ceilingheight = TO_FIXED(128),
        .floorpic      = 0, // Stone stairs
        .ceilingpic    = 2,
        .lightlevel    = 160,
        .validcount    = 0
    },
    // Sector 2: Sunken Toxic Slime Pit Courtyard (Sunken floor -32, High ceiling 256!)
    {
        .floorheight   = TO_FIXED(-32),
        .ceilingheight = TO_FIXED(256),
        .floorpic      = 1, // Acid nukage slime!
        .ceilingpic    = 2,
        .lightlevel    = 250, // Bright outdoor courtyard
        .validcount    = 0
    },
    // Sector 3: Computer Terminal Alcove (Elevated floor +16)
    {
        .floorheight   = TO_FIXED(16),
        .ceilingheight = TO_FIXED(112),
        .floorpic      = 0,
        .ceilingpic    = 2,
        .lightlevel    = 190,
        .validcount    = 0
    }
};

#define NUM_SECTORS (sizeof(map_sectors) / sizeof(map_sectors[0]))

// ------------------------------------------------------------------------------
// 3. Sidedefs (Texture Mappings)
// ------------------------------------------------------------------------------
static side_t map_sidedefs[] __attribute__((unused)) = {
    // 0..8: Sector 0 (Hangar) Sidedefs
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] }, // 0: South
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] }, // 1: West
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] }, // 2: NW wing
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = 1, .sector = &map_sectors[0] }, // 3: Portal to stairs
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[1] }, // 4: Portal back from stairs
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] }, // 5: NE wing
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = 1, .sector = &map_sectors[0] }, // 6: Portal to comp
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] }, // 7: Portal back from comp
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] }, // 8: East lower

    // 9..12: Sector 1 (Stairs) Sidedefs
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[1] }, // 9: West
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = 0, .bottomtexture = 1, .sector = &map_sectors[1] }, // 10: Portal to courtyard
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = 0, .bottomtexture = 1, .sector = &map_sectors[2] }, // 11: Portal back from courtyard
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[1] }, // 12: East

    // 13..17: Sector 2 (Courtyard) Sidedefs
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] }, // 13: SW wing
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] }, // 14: West
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] }, // 15: North
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] }, // 16: East
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] }, // 17: SE wing

    // 18..20: Sector 3 (Computer Room) Sidedefs
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 2, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] }, // 18: South
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 2, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] }, // 19: East
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 2, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] }  // 20: North
};

#define NUM_SIDEDEFS (sizeof(map_sidedefs) / sizeof(map_sidedefs[0]))

// ------------------------------------------------------------------------------
// 4. Linedefs (One-Sided Solid Walls vs. Two-Sided Step Portals)
// ------------------------------------------------------------------------------
static line_t map_lines[] __attribute__((unused)) = {
    // --- Sector 0: Starting Hangar Enclosure ---
    // Line 0: Hangar South Wall (Solid) - V1 -> V0
    {
        .v1 = (vertex_t*)&map_vertices[1], .v2 = (vertex_t*)&map_vertices[0],
        .dx = TO_FIXED(-400), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[0], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },
    // Line 1: Hangar West Wall (Solid) - V0 -> V3
    {
        .v1 = (vertex_t*)&map_vertices[0], .v2 = (vertex_t*)&map_vertices[3],
        .dx = TO_FIXED(0), .dy = TO_FIXED(300), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[1], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },
    // Line 2: Hangar NW Wing Wall (Solid) - V3 -> V4
    {
        .v1 = (vertex_t*)&map_vertices[3], .v2 = (vertex_t*)&map_vertices[4],
        .dx = TO_FIXED(100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[2], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },
    // Line 3: Hangar to Stairway Portal (Two-Sided Step) - V4 -> V5
    {
        .v1 = (vertex_t*)&map_vertices[4], .v2 = (vertex_t*)&map_vertices[5],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .sidenum = { &map_sidedefs[3], &map_sidedefs[4] },
        .frontsector = &map_sectors[0], .backsector = &map_sectors[1]
    },
    // Line 4: Hangar NE Wing Wall (Solid) - V5 -> V2
    {
        .v1 = (vertex_t*)&map_vertices[5], .v2 = (vertex_t*)&map_vertices[2],
        .dx = TO_FIXED(100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[5], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },
    // Line 5: Hangar to Computer Room Portal (Two-Sided Step) - V2 -> V12
    {
        .v1 = (vertex_t*)&map_vertices[2], .v2 = (vertex_t*)&map_vertices[12],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-200), .flags = ML_TWOSIDED,
        .sidenum = { &map_sidedefs[6], &map_sidedefs[7] },
        .frontsector = &map_sectors[0], .backsector = &map_sectors[3]
    },
    // Line 6: Hangar East Wall below Computer Room (Solid) - V12 -> V1
    {
        .v1 = (vertex_t*)&map_vertices[12], .v2 = (vertex_t*)&map_vertices[1],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-100), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[8], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },

    // --- Sector 1: Stairway Enclosure ---
    // Line 7: Stairway West Wall (Solid) - V4 -> V7
    {
        .v1 = (vertex_t*)&map_vertices[4], .v2 = (vertex_t*)&map_vertices[7],
        .dx = TO_FIXED(0), .dy = TO_FIXED(150), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[9], NULL },
        .frontsector = &map_sectors[1], .backsector = NULL
    },
    // Line 8: Stairway to Courtyard Portal (Two-Sided Drop) - V7 -> V6
    {
        .v1 = (vertex_t*)&map_vertices[7], .v2 = (vertex_t*)&map_vertices[6],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .sidenum = { &map_sidedefs[10], &map_sidedefs[11] },
        .frontsector = &map_sectors[1], .backsector = &map_sectors[2]
    },
    // Line 9: Stairway East Wall (Solid) - V6 -> V5
    {
        .v1 = (vertex_t*)&map_vertices[6], .v2 = (vertex_t*)&map_vertices[5],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-150), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[12], NULL },
        .frontsector = &map_sectors[1], .backsector = NULL
    },

    // --- Sector 2: Sunken Toxic Slime Courtyard Enclosure ---
    // Line 10: Courtyard SW Wing Wall (Solid) - V7 -> V8
    {
        .v1 = (vertex_t*)&map_vertices[7], .v2 = (vertex_t*)&map_vertices[8],
        .dx = TO_FIXED(-250), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[13], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    // Line 11: Courtyard West Wall (Solid) - V8 -> V11
    {
        .v1 = (vertex_t*)&map_vertices[8], .v2 = (vertex_t*)&map_vertices[11],
        .dx = TO_FIXED(0), .dy = TO_FIXED(350), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[14], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    // Line 12: Courtyard North Wall (Solid) - V11 -> V10
    {
        .v1 = (vertex_t*)&map_vertices[11], .v2 = (vertex_t*)&map_vertices[10],
        .dx = TO_FIXED(700), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[15], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    // Line 13: Courtyard East Wall (Solid) - V10 -> V9
    {
        .v1 = (vertex_t*)&map_vertices[10], .v2 = (vertex_t*)&map_vertices[9],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-350), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[16], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    // Line 14: Courtyard SE Wing Wall (Solid) - V9 -> V6
    {
        .v1 = (vertex_t*)&map_vertices[9], .v2 = (vertex_t*)&map_vertices[6],
        .dx = TO_FIXED(-250), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[17], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },

    // --- Sector 3: Computer Terminal Control Room Enclosure ---
    // Line 15: Computer Room South Wall (Solid) - V13 -> V12
    {
        .v1 = (vertex_t*)&map_vertices[13], .v2 = (vertex_t*)&map_vertices[12],
        .dx = TO_FIXED(-200), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[18], NULL },
        .frontsector = &map_sectors[3], .backsector = NULL
    },
    // Line 16: Computer Room East Wall (Solid) - V14 -> V13
    {
        .v1 = (vertex_t*)&map_vertices[14], .v2 = (vertex_t*)&map_vertices[13],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-200), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[19], NULL },
        .frontsector = &map_sectors[3], .backsector = NULL
    },
    // Line 17: Computer Room North Wall (Solid) - V2 -> V14
    {
        .v1 = (vertex_t*)&map_vertices[2], .v2 = (vertex_t*)&map_vertices[14],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[20], NULL },
        .frontsector = &map_sectors[3], .backsector = NULL
    }
};

#define NUM_LINES (sizeof(map_lines) / sizeof(map_lines[0]))

// ------------------------------------------------------------------------------
// 5. 2D BSP Tree Nodes (Partition Lines & Subsectors)
// ------------------------------------------------------------------------------
static const node_t map_nodes[] __attribute__((unused)) = {
    // Root Node 0: Horizontal partition at Y = 100 dividing Hangar from Courtyard
    {
        .x  = TO_FIXED(-350), .y  = TO_FIXED(100),
        .dx = TO_FIXED(700),  .dy = TO_FIXED(0),
        // Front bbox (Courtyard & Stairs: Y >= 100)
        .bbox = {
            { TO_FIXED(600), TO_FIXED(100), TO_FIXED(-350), TO_FIXED(400) },
            // Back bbox (Hangar & Comp: Y <= 100)
            { TO_FIXED(100), TO_FIXED(-200), TO_FIXED(-200), TO_FIXED(400) }
        },
        .children = { 1, 2 } // Child 1: Courtyard sub-tree, Child 2: Hangar sub-tree
    },
    // Node 1: Vertical partition in Courtyard at X = 0
    {
        .x  = TO_FIXED(0),    .y  = TO_FIXED(100),
        .dx = TO_FIXED(0),    .dy = TO_FIXED(500),
        .bbox = {
            { TO_FIXED(600), TO_FIXED(100), TO_FIXED(0), TO_FIXED(350) },
            { TO_FIXED(600), TO_FIXED(100), TO_FIXED(-350), TO_FIXED(0) }
        },
        .children = { NF_SUBSECTOR | 2, NF_SUBSECTOR | 1 } // Subsectors 2 & 1
    },
    // Node 2: Vertical partition between Hangar and Computer Alcove at X = 200
    {
        .x  = TO_FIXED(200),  .y  = TO_FIXED(-200),
        .dx = TO_FIXED(0),    .dy = TO_FIXED(300),
        .bbox = {
            { TO_FIXED(100), TO_FIXED(-100), TO_FIXED(200), TO_FIXED(400) },
            { TO_FIXED(100), TO_FIXED(-200), TO_FIXED(-200), TO_FIXED(200) }
        },
        .children = { NF_SUBSECTOR | 3, NF_SUBSECTOR | 0 } // Subsectors 3 & 0
    }
};

#define NUM_NODES (sizeof(map_nodes) / sizeof(map_nodes[0]))

// ------------------------------------------------------------------------------
// 6. Initial Mobjs Spawn Table (Entities in Level)
// ------------------------------------------------------------------------------
typedef struct {
    fixed_t x, y, z;
    mobjtype_t type;
    angle_t angle;
} mobj_spawn_t;

static const mobj_spawn_t map_spawns[] __attribute__((unused)) = {
    // Player Spawn in Hangar
    { TO_FIXED(0), TO_FIXED(-100), TO_FIXED(0), MT_PLAYER, ANG90 },
    // Imp Demon: Guarding the top of the stairs
    { TO_FIXED(0), TO_FIXED(180), TO_FIXED(32), MT_IMP, ANG270 },
    // Pinky Demon: Roaming the sunken toxic courtyard
    { TO_FIXED(150), TO_FIXED(380), TO_FIXED(-32), MT_DEMON, ANG180 },
    // Shotgun Guy: Camping inside the Computer Room
    { TO_FIXED(300), TO_FIXED(0), TO_FIXED(16), MT_SHOTGUNGUY, ANG180 }
};

#define NUM_SPAWNS (sizeof(map_spawns) / sizeof(map_spawns[0]))

#endif // DOOM_MAP_H
