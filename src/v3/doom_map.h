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
    { TO_FIXED(-200), TO_FIXED(-200) }, // 0
    { TO_FIXED( 200), TO_FIXED(-200) }, // 1
    { TO_FIXED( 200), TO_FIXED( 100) }, // 2
    { TO_FIXED(-200), TO_FIXED( 100) }, // 3

    // Sector 1: Stairway Portal to Courtyard (4..7)
    { TO_FIXED(-100), TO_FIXED( 100) }, // 4 (Shared with 3..2)
    { TO_FIXED( 100), TO_FIXED( 100) }, // 5
    { TO_FIXED( 100), TO_FIXED( 250) }, // 6
    { TO_FIXED(-100), TO_FIXED( 250) }, // 7

    // Sector 2: Sunken Toxic Slime Courtyard (8..11)
    { TO_FIXED(-350), TO_FIXED( 250) }, // 8
    { TO_FIXED( 350), TO_FIXED( 250) }, // 9
    { TO_FIXED( 350), TO_FIXED( 600) }, // 10
    { TO_FIXED(-350), TO_FIXED( 600) }, // 11

    // Sector 3: Computer Terminal Control Room (12..15)
    { TO_FIXED( 100), TO_FIXED(-100) }, // 12 (Right alcove off Hangar)
    { TO_FIXED( 400), TO_FIXED(-100) }, // 13
    { TO_FIXED( 400), TO_FIXED( 100) }, // 14
    { TO_FIXED( 200), TO_FIXED( 100) }  // 15
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
    // 0..3: Hangar solid outer walls (STARTAN3: tex 0)
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 0, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[0] },

    // 4..5: Stairway Portal Sidedefs (Two-sided step from Sector 0 to 1)
    // Front side (facing Hangar): Lower texture draws step face (BROWN96: tex 1)
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = 1, .sector = &map_sectors[0] },
    // Back side (facing Stairs):
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[1] },

    // 6..7: Stairway Portal Sidedefs to Courtyard (Two-sided step from Sector 1 to 2)
    // Step down from +32 to -32: Lower texture draws deep drop face (BROWN96: tex 1)
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = 0, .bottomtexture = 1, .sector = &map_sectors[1] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = 0, .bottomtexture = 1, .sector = &map_sectors[2] },

    // 8..11: Courtyard Outer Perimeter Walls (BROWN96: tex 1)
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 1, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[2] },

    // 12..15: Computer Terminal Alcove Walls (COMP01: tex 2)
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 2, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 2, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = 2, .toptexture = -1, .bottomtexture = -1, .sector = &map_sectors[3] },
    { .textureoffset = 0, .rowoffset = 0, .midtexture = -1, .toptexture = -1, .bottomtexture = 1, .sector = &map_sectors[0] } // Portal
};

#define NUM_SIDEDEFS (sizeof(map_sidedefs) / sizeof(map_sidedefs[0]))

// ------------------------------------------------------------------------------
// 4. Linedefs (One-Sided Solid Walls vs. Two-Sided Step Portals)
// ------------------------------------------------------------------------------
static line_t map_lines[] __attribute__((unused)) = {
    // Line 0: Hangar South Wall (Solid)
    {
        .v1 = (vertex_t*)&map_vertices[0], .v2 = (vertex_t*)&map_vertices[1],
        .dx = TO_FIXED(400), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[0], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },
    // Line 1: Hangar West Wall (Solid)
    {
        .v1 = (vertex_t*)&map_vertices[3], .v2 = (vertex_t*)&map_vertices[0],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-300), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[1], NULL },
        .frontsector = &map_sectors[0], .backsector = NULL
    },
    // Line 2: Hangar Portal to Stairway (Two-Sided Step!)
    {
        .v1 = (vertex_t*)&map_vertices[4], .v2 = (vertex_t*)&map_vertices[5],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .sidenum = { &map_sidedefs[4], &map_sidedefs[5] },
        .frontsector = &map_sectors[0], .backsector = &map_sectors[1]
    },
    // Line 3: Stairway Side Walls
    {
        .v1 = (vertex_t*)&map_vertices[4], .v2 = (vertex_t*)&map_vertices[7],
        .dx = TO_FIXED(0), .dy = TO_FIXED(150), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[2], NULL },
        .frontsector = &map_sectors[1], .backsector = NULL
    },
    {
        .v1 = (vertex_t*)&map_vertices[6], .v2 = (vertex_t*)&map_vertices[5],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-150), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[3], NULL },
        .frontsector = &map_sectors[1], .backsector = NULL
    },
    // Line 4: Stairway to Toxic Courtyard Portal (Two-Sided Ledge Drop!)
    {
        .v1 = (vertex_t*)&map_vertices[7], .v2 = (vertex_t*)&map_vertices[6],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .sidenum = { &map_sidedefs[6], &map_sidedefs[7] },
        .frontsector = &map_sectors[1], .backsector = &map_sectors[2]
    },
    // Line 5..7: Toxic Courtyard Perimeter Walls (Solid)
    {
        .v1 = (vertex_t*)&map_vertices[8], .v2 = (vertex_t*)&map_vertices[11],
        .dx = TO_FIXED(0), .dy = TO_FIXED(350), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[8], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    {
        .v1 = (vertex_t*)&map_vertices[11], .v2 = (vertex_t*)&map_vertices[10],
        .dx = TO_FIXED(700), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[9], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    {
        .v1 = (vertex_t*)&map_vertices[10], .v2 = (vertex_t*)&map_vertices[9],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-350), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[10], NULL },
        .frontsector = &map_sectors[2], .backsector = NULL
    },
    // Line 8..10: Computer Terminal Control Room (Solid)
    {
        .v1 = (vertex_t*)&map_vertices[12], .v2 = (vertex_t*)&map_vertices[13],
        .dx = TO_FIXED(300), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[12], NULL },
        .frontsector = &map_sectors[3], .backsector = NULL
    },
    {
        .v1 = (vertex_t*)&map_vertices[13], .v2 = (vertex_t*)&map_vertices[14],
        .dx = TO_FIXED(0), .dy = TO_FIXED(200), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[13], NULL },
        .frontsector = &map_sectors[3], .backsector = NULL
    },
    {
        .v1 = (vertex_t*)&map_vertices[14], .v2 = (vertex_t*)&map_vertices[15],
        .dx = TO_FIXED(-200), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .sidenum = { &map_sidedefs[14], NULL },
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
    // Imp Demon 1: Guarding the top of the stairs
    { TO_FIXED(0), TO_FIXED(180), TO_FIXED(32), MT_IMP, ANG270 },
    // Imp Demon 2: Roaming the sunken toxic courtyard
    { TO_FIXED(150), TO_FIXED(380), TO_FIXED(-32), MT_IMP, ANG180 },
    // Imp Demon 3: Camping inside the Computer Room
    { TO_FIXED(300), TO_FIXED(0), TO_FIXED(16), MT_IMP, ANG180 },
    // Toxic Barrels near the stairs
    { TO_FIXED(-60), TO_FIXED(80), TO_FIXED(0), MT_BARREL, 0 },
    { TO_FIXED(60), TO_FIXED(80), TO_FIXED(0), MT_BARREL, 0 },
    // Health Medkits in Computer Alcove
    { TO_FIXED(350), TO_FIXED(-60), TO_FIXED(16), MT_MEDKIT, 0 },
    { TO_FIXED(350), TO_FIXED(60), TO_FIXED(16), MT_MEDKIT, 0 }
};

#define NUM_SPAWNS (sizeof(map_spawns) / sizeof(map_spawns[0]))

#endif // DOOM_MAP_H
