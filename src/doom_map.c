#include "doom_map.h"
#include "p_mobj.h"
#include <string.h>

extern int player_health;
extern int player_ammo;
extern int player_angle_idx;

const doom_map_t *cur_map = NULL;
int cur_map_idx = 0;

// ==============================================================================
// MAP 1: E1M1 HANGAR & TOXIC COURTYARD
// ==============================================================================

static const vertex_t map1_vertices[19] = {
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
    { TO_FIXED( 400), TO_FIXED( 100) }, // 14: Comp NE

    // Sector 4: Courtyard Center Armor Pedestal Structure (15..18)
    { TO_FIXED(-50), TO_FIXED( 380) }, // 15: Pedestal SW
    { TO_FIXED( 50), TO_FIXED( 380) }, // 16: Pedestal SE
    { TO_FIXED( 50), TO_FIXED( 480) }, // 17: Pedestal NE
    { TO_FIXED(-50), TO_FIXED( 480) }  // 18: Pedestal NW
};

static const sector_t map1_sectors[4] = {
    // Sector 0: Starting Starport Hangar
    {
        .floorheight   = TO_FIXED(0),
        .ceilingheight = TO_FIXED(128),
        .floorpic      = 72,  // Brown stone floor flat
        .ceilingpic    = 198, // Authentic tech blue ceiling
        .lightlevel    = 180,
        .validcount    = 0
    },
    // Sector 1: Elevated Staircase Steps (+32 units)
    {
        .floorheight   = TO_FIXED(32),
        .ceilingheight = TO_FIXED(128),
        .floorpic      = 74,  // Authentic stone stairs tread
        .ceilingpic    = 198, // Tech blue ceiling
        .lightlevel    = 160,
        .validcount    = 0
    },
    // Sector 2: Sunken Toxic Slime Courtyard (-32 floor, 256 sky ceiling)
    {
        .floorheight   = TO_FIXED(-32),
        .ceilingheight = TO_FIXED(256),
        .floorpic      = 116, // Bright toxic nukage acid slime
        .ceilingpic    = 244, // Open outdoor sky
        .lightlevel    = 250, // Bright outdoor courtyard
        .validcount    = 0
    },
    // Sector 3: Computer Terminal Alcove (+16 floor)
    {
        .floorheight   = TO_FIXED(16),
        .ceilingheight = TO_FIXED(112),
        .floorpic      = 72,  // Brown stone floor
        .ceilingpic    = 198, // Tech blue ceiling
        .lightlevel    = 190,
        .validcount    = 0
    }
};

static const line_t map1_lines[22] = {
    // Line 0: Hangar South Wall (Solid) - V1 -> V0
    {
        .v1 = (vertex_t*)&map1_vertices[1], .v2 = (vertex_t*)&map1_vertices[0],
        .dx = TO_FIXED(-400), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = NULL
    },
    // Line 1: Hangar West Wall (Solid) - V0 -> V3
    {
        .v1 = (vertex_t*)&map1_vertices[0], .v2 = (vertex_t*)&map1_vertices[3],
        .dx = TO_FIXED(0), .dy = TO_FIXED(300), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = NULL
    },
    // Line 2: Hangar NW Wing Wall (Solid) - V3 -> V4
    {
        .v1 = (vertex_t*)&map1_vertices[3], .v2 = (vertex_t*)&map1_vertices[4],
        .dx = TO_FIXED(100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = NULL
    },
    // Line 3: Hangar to Stairway Portal (Two-Sided Step) - V4 -> V5
    {
        .v1 = (vertex_t*)&map1_vertices[4], .v2 = (vertex_t*)&map1_vertices[5],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = (sector_t*)&map1_sectors[1]
    },
    // Line 4: Hangar NE Wing Wall (Solid) - V5 -> V2
    {
        .v1 = (vertex_t*)&map1_vertices[5], .v2 = (vertex_t*)&map1_vertices[2],
        .dx = TO_FIXED(100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = NULL
    },
    // Line 5: Hangar to Computer Room Portal (Two-Sided Step) - V2 -> V12
    {
        .v1 = (vertex_t*)&map1_vertices[2], .v2 = (vertex_t*)&map1_vertices[12],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-200), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = (sector_t*)&map1_sectors[3]
    },
    // Line 6: Hangar East Lower Wall (Solid) - V12 -> V1
    {
        .v1 = (vertex_t*)&map1_vertices[12], .v2 = (vertex_t*)&map1_vertices[1],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-100), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[0], .backsector = NULL
    },
    // Line 7: Stairway West Flank (Solid) - V4 -> V7
    {
        .v1 = (vertex_t*)&map1_vertices[4], .v2 = (vertex_t*)&map1_vertices[7],
        .dx = TO_FIXED(0), .dy = TO_FIXED(150), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[1], .backsector = NULL
    },
    // Line 8: Stairway to Courtyard Portal (Two-Sided Step) - V7 -> V6
    {
        .v1 = (vertex_t*)&map1_vertices[7], .v2 = (vertex_t*)&map1_vertices[6],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map1_sectors[1], .backsector = (sector_t*)&map1_sectors[2]
    },
    // Line 9: Stairway East Flank (Solid) - V6 -> V5
    {
        .v1 = (vertex_t*)&map1_vertices[6], .v2 = (vertex_t*)&map1_vertices[5],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-150), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[1], .backsector = NULL
    },
    // Line 10: Courtyard SW Wing (Solid) - V7 -> V8
    {
        .v1 = (vertex_t*)&map1_vertices[7], .v2 = (vertex_t*)&map1_vertices[8],
        .dx = TO_FIXED(-250), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 11: Courtyard West Wall (Solid) - V8 -> V11
    {
        .v1 = (vertex_t*)&map1_vertices[8], .v2 = (vertex_t*)&map1_vertices[11],
        .dx = TO_FIXED(0), .dy = TO_FIXED(350), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 12: Courtyard North Wall (Solid) - V11 -> V10
    {
        .v1 = (vertex_t*)&map1_vertices[11], .v2 = (vertex_t*)&map1_vertices[10],
        .dx = TO_FIXED(700), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 13: Courtyard East Wall (Solid) - V10 -> V9
    {
        .v1 = (vertex_t*)&map1_vertices[10], .v2 = (vertex_t*)&map1_vertices[9],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-350), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 14: Courtyard SE Wing (Solid) - V9 -> V6
    {
        .v1 = (vertex_t*)&map1_vertices[9], .v2 = (vertex_t*)&map1_vertices[6],
        .dx = TO_FIXED(-250), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 15: Computer Room South Wall (Solid) - V13 -> V12
    {
        .v1 = (vertex_t*)&map1_vertices[13], .v2 = (vertex_t*)&map1_vertices[12],
        .dx = TO_FIXED(-200), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[3], .backsector = NULL
    },
    // Line 16: Computer Room East Wall (Solid) - V14 -> V13
    {
        .v1 = (vertex_t*)&map1_vertices[14], .v2 = (vertex_t*)&map1_vertices[13],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-200), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[3], .backsector = NULL
    },
    // Line 17: Computer Room North Wall (Solid) - V2 -> V14
    {
        .v1 = (vertex_t*)&map1_vertices[2], .v2 = (vertex_t*)&map1_vertices[14],
        .dx = TO_FIXED(200), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[3], .backsector = NULL
    },
    // Line 18: Pedestal South Face (Solid) - V15 -> V16
    {
        .v1 = (vertex_t*)&map1_vertices[15], .v2 = (vertex_t*)&map1_vertices[16],
        .dx = TO_FIXED(100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 19: Pedestal East Face (Solid) - V16 -> V17
    {
        .v1 = (vertex_t*)&map1_vertices[16], .v2 = (vertex_t*)&map1_vertices[17],
        .dx = TO_FIXED(0), .dy = TO_FIXED(100), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 20: Pedestal North Face (Solid) - V17 -> V18
    {
        .v1 = (vertex_t*)&map1_vertices[17], .v2 = (vertex_t*)&map1_vertices[18],
        .dx = TO_FIXED(-100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    },
    // Line 21: Pedestal West Face (Solid) - V18 -> V15
    {
        .v1 = (vertex_t*)&map1_vertices[18], .v2 = (vertex_t*)&map1_vertices[15],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-100), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map1_sectors[2], .backsector = NULL
    }
};

static const mobj_spawn_t map1_spawns[5] = {
    // Player Spawn in Hangar
    { TO_FIXED(0), TO_FIXED(-100), TO_FIXED(0), MT_PLAYER, ANG90 },
    // Imp Demon: Guarding the top of the stairs
    { TO_FIXED(0), TO_FIXED(180), TO_FIXED(32), MT_IMP, ANG270 },
    // Pinky Demon: Roaming the sunken toxic courtyard
    { TO_FIXED(150), TO_FIXED(380), TO_FIXED(-32), MT_DEMON, ANG180 },
    // Imp Demon #2: Guarding the top of the Armor Pedestal
    { TO_FIXED(0), TO_FIXED(430), TO_FIXED(32), MT_IMP, ANG270 },
    // Shotgun Guy: Camping inside the Computer Room
    { TO_FIXED(300), TO_FIXED(0), TO_FIXED(16), MT_SHOTGUNGUY, ANG180 }
};

// ==============================================================================
// MAP 2: MAP02 UNDERHALLS & TOXIC CANALS
// ==============================================================================

static const vertex_t map2_vertices[17] = {
    // Sector 0: Central Hub / Walkway (0..3)
    { TO_FIXED(-120), TO_FIXED(-120) }, // 0: Hub SW
    { TO_FIXED( 120), TO_FIXED(-120) }, // 1: Hub SE
    { TO_FIXED( 120), TO_FIXED( 120) }, // 2: Hub NE
    { TO_FIXED(-120), TO_FIXED( 120) }, // 3: Hub NW

    // Sector 1: Toxic Sludge Canal Outer Perimeter (4..6)
    { TO_FIXED(-120), TO_FIXED( 280) }, // 4: Canal North NW
    { TO_FIXED( 280), TO_FIXED( 280) }, // 5: Canal North NE
    { TO_FIXED( 280), TO_FIXED(-120) }, // 6: Canal East SE

    // Sector 2: Raised Sniper Battlement (7..8)
    { TO_FIXED(-260), TO_FIXED(-120) }, // 7: Sniper SW
    { TO_FIXED(-260), TO_FIXED( 280) }, // 8: Sniper NW

    // Sector 3: Armory Bunker (9..12)
    { TO_FIXED( -50), TO_FIXED(-240) }, // 9: Armory SW
    { TO_FIXED(  50), TO_FIXED(-240) }, // 10: Armory SE
    { TO_FIXED(  50), TO_FIXED(-120) }, // 11: Armory NE / Hub S2
    { TO_FIXED( -50), TO_FIXED(-120) }, // 12: Armory NW / Hub S1

    // Canal Center Obstacle Pillar (13..16)
    { TO_FIXED(  40), TO_FIXED( 180) }, // 13: Pillar SW
    { TO_FIXED( 120), TO_FIXED( 180) }, // 14: Pillar SE
    { TO_FIXED( 120), TO_FIXED( 240) }, // 15: Pillar NE
    { TO_FIXED(  40), TO_FIXED( 240) }  // 16: Pillar NW
};

static const sector_t map2_sectors[4] = {
    // Sector 0: Central Starting Hub
    {
        .floorheight   = TO_FIXED(0),
        .ceilingheight = TO_FIXED(128),
        .floorpic      = 72,  // Brown stone floor
        .ceilingpic    = 198, // Tech blue ceiling
        .lightlevel    = 180,
        .validcount    = 0
    },
    // Sector 1: Toxic Sludge Canal (-32 floor, high sky)
    {
        .floorheight   = TO_FIXED(-32),
        .ceilingheight = TO_FIXED(256),
        .floorpic      = 116, // Bright toxic slime
        .ceilingpic    = 244, // Outdoor sky
        .lightlevel    = 240,
        .validcount    = 0
    },
    // Sector 2: Raised Sniper Battlement (+48 floor)
    {
        .floorheight   = TO_FIXED(48),
        .ceilingheight = TO_FIXED(144),
        .floorpic      = 74,  // Stone ledge
        .ceilingpic    = 198, // Tech blue ceiling
        .lightlevel    = 190,
        .validcount    = 0
    },
    // Sector 3: Dark Armory Bunker (+16 floor)
    {
        .floorheight   = TO_FIXED(16),
        .ceilingheight = TO_FIXED(112),
        .floorpic      = 72,  // Brown stone floor
        .ceilingpic    = 198, // Tech blue ceiling
        .lightlevel    = 160,
        .validcount    = 0
    }
};

static const line_t map2_lines[20] = {
    // 0: Hub South (West wing) - V12 -> V0
    {
        .v1 = (vertex_t*)&map2_vertices[12], .v2 = (vertex_t*)&map2_vertices[0],
        .dx = TO_FIXED(-70), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[0], .backsector = NULL
    },
    // 1: Hub to Armory Portal - V11 -> V12
    {
        .v1 = (vertex_t*)&map2_vertices[11], .v2 = (vertex_t*)&map2_vertices[12],
        .dx = TO_FIXED(-100), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map2_sectors[0], .backsector = (sector_t*)&map2_sectors[3]
    },
    // 2: Hub South (East wing) - V1 -> V11
    {
        .v1 = (vertex_t*)&map2_vertices[1], .v2 = (vertex_t*)&map2_vertices[11],
        .dx = TO_FIXED(-70), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[0], .backsector = NULL
    },
    // 3: Hub to Canal East Portal - V1 -> V2
    {
        .v1 = (vertex_t*)&map2_vertices[1], .v2 = (vertex_t*)&map2_vertices[2],
        .dx = TO_FIXED(0), .dy = TO_FIXED(240), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map2_sectors[0], .backsector = (sector_t*)&map2_sectors[1]
    },
    // 4: Hub to Canal North Portal - V2 -> V3
    {
        .v1 = (vertex_t*)&map2_vertices[2], .v2 = (vertex_t*)&map2_vertices[3],
        .dx = TO_FIXED(-240), .dy = TO_FIXED(0), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map2_sectors[0], .backsector = (sector_t*)&map2_sectors[1]
    },
    // 5: Hub to Sniper Ledge Portal - V3 -> V0
    {
        .v1 = (vertex_t*)&map2_vertices[3], .v2 = (vertex_t*)&map2_vertices[0],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-240), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map2_sectors[0], .backsector = (sector_t*)&map2_sectors[2]
    },
    // 6: Sniper West Wall - V7 -> V8
    {
        .v1 = (vertex_t*)&map2_vertices[7], .v2 = (vertex_t*)&map2_vertices[8],
        .dx = TO_FIXED(0), .dy = TO_FIXED(400), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[2], .backsector = NULL
    },
    // 7: Sniper North Wall - V8 -> V4
    {
        .v1 = (vertex_t*)&map2_vertices[8], .v2 = (vertex_t*)&map2_vertices[4],
        .dx = TO_FIXED(140), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[2], .backsector = NULL
    },
    // 8: Sniper South Wall - V0 -> V7
    {
        .v1 = (vertex_t*)&map2_vertices[0], .v2 = (vertex_t*)&map2_vertices[7],
        .dx = TO_FIXED(-140), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[2], .backsector = NULL
    },
    // 9: Sniper to Canal North Ledge - V4 -> V3
    {
        .v1 = (vertex_t*)&map2_vertices[4], .v2 = (vertex_t*)&map2_vertices[3],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-160), .flags = ML_TWOSIDED,
        .frontsector = (sector_t*)&map2_sectors[2], .backsector = (sector_t*)&map2_sectors[1]
    },
    // 10: Canal North Outer Wall - V4 -> V5
    {
        .v1 = (vertex_t*)&map2_vertices[4], .v2 = (vertex_t*)&map2_vertices[5],
        .dx = TO_FIXED(400), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    },
    // 11: Canal East Outer Wall - V5 -> V6
    {
        .v1 = (vertex_t*)&map2_vertices[5], .v2 = (vertex_t*)&map2_vertices[6],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-400), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    },
    // 12: Canal South Outer Wall - V6 -> V1
    {
        .v1 = (vertex_t*)&map2_vertices[6], .v2 = (vertex_t*)&map2_vertices[1],
        .dx = TO_FIXED(-160), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    },
    // 13: Armory West Wall - V12 -> V9
    {
        .v1 = (vertex_t*)&map2_vertices[12], .v2 = (vertex_t*)&map2_vertices[9],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-120), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[3], .backsector = NULL
    },
    // 14: Armory South Wall - V9 -> V10
    {
        .v1 = (vertex_t*)&map2_vertices[9], .v2 = (vertex_t*)&map2_vertices[10],
        .dx = TO_FIXED(100), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[3], .backsector = NULL
    },
    // 15: Armory East Wall - V10 -> V11
    {
        .v1 = (vertex_t*)&map2_vertices[10], .v2 = (vertex_t*)&map2_vertices[11],
        .dx = TO_FIXED(0), .dy = TO_FIXED(120), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[3], .backsector = NULL
    },
    // 16: Pillar South - V13 -> V14
    {
        .v1 = (vertex_t*)&map2_vertices[13], .v2 = (vertex_t*)&map2_vertices[14],
        .dx = TO_FIXED(80), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    },
    // 17: Pillar East - V14 -> V15
    {
        .v1 = (vertex_t*)&map2_vertices[14], .v2 = (vertex_t*)&map2_vertices[15],
        .dx = TO_FIXED(0), .dy = TO_FIXED(60), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    },
    // 18: Pillar North - V15 -> V16
    {
        .v1 = (vertex_t*)&map2_vertices[15], .v2 = (vertex_t*)&map2_vertices[16],
        .dx = TO_FIXED(-80), .dy = TO_FIXED(0), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    },
    // 19: Pillar West - V16 -> V13
    {
        .v1 = (vertex_t*)&map2_vertices[16], .v2 = (vertex_t*)&map2_vertices[13],
        .dx = TO_FIXED(0), .dy = TO_FIXED(-60), .flags = ML_BLOCKING,
        .frontsector = (sector_t*)&map2_sectors[1], .backsector = NULL
    }
};

static const mobj_spawn_t map2_spawns[5] = {
    // Player Spawn on Central Hub
    { TO_FIXED(0), TO_FIXED(0), TO_FIXED(0), MT_PLAYER, ANG90 },
    // Imp on Raised Sniper Battlement overlooking hub
    { TO_FIXED(-190), TO_FIXED(80), TO_FIXED(48), MT_IMP, 0 },
    // Pinky Demon prowling the Toxic Canal East
    { TO_FIXED(200), TO_FIXED(80), TO_FIXED(-32), MT_DEMON, ANG180 },
    // Imp in Toxic Canal North
    { TO_FIXED(80), TO_FIXED(240), TO_FIXED(-32), MT_IMP, ANG270 },
    // Shotgun Guy guarding the South Armory Bunker
    { TO_FIXED(0), TO_FIXED(-180), TO_FIXED(16), MT_SHOTGUNGUY, ANG90 }
};

// ==============================================================================
// MAP MASTER TABLE & MAP LOADING API
// ==============================================================================

const doom_map_t maps[NUM_MAPS] = {
    {
        .name             = "E1M1",
        .title            = "HANGAR",
        .vertices         = map1_vertices,
        .num_vertices     = 19,
        .sectors          = map1_sectors,
        .num_sectors      = 4,
        .lines            = map1_lines,
        .num_lines        = 22,
        .spawns           = map1_spawns,
        .num_spawns       = 5,
        .start_x          = TO_FIXED(0),
        .start_y          = TO_FIXED(-100),
        .start_z          = TO_FIXED(0),
        .start_angle_idx  = 16
    },
    {
        .name             = "MAP02",
        .title            = "CANALS",
        .vertices         = map2_vertices,
        .num_vertices     = 17,
        .sectors          = map2_sectors,
        .num_sectors      = 4,
        .lines            = map2_lines,
        .num_lines        = 20,
        .spawns           = map2_spawns,
        .num_spawns       = 5,
        .start_x          = TO_FIXED(0),
        .start_y          = TO_FIXED(0),
        .start_z          = TO_FIXED(0),
        .start_angle_idx  = 16
    }
};

void P_LoadMap(int map_idx) {
    if (map_idx < 0 || map_idx >= NUM_MAPS) map_idx = 0;
    cur_map_idx = map_idx;
    cur_map = &maps[map_idx];

    // Initialize mobjs and player for this level
    P_InitMobjs();

    if (player_mobj) {
        player_mobj->x = cur_map->start_x;
        player_mobj->y = cur_map->start_y;
        player_mobj->z = cur_map->start_z;
    }
    player_angle_idx = cur_map->start_angle_idx;
    player_health = 100;
    player_ammo = 50;

    // Clear viewport to prevent visual remnants
    volatile uint8_t *vram = (volatile uint8_t *)VGA_VRAM_BASE;
    for (int y = 0; y < VIEWPORT_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            vram[y * SCREEN_W + x] = 0;
        }
    }
}

void P_InitMaps(void) {
    P_LoadMap(0);
}

// ------------------------------------------------------------------------------
// Wall Collision & Distance Clipping: Prevents clipping through/into solid walls
// ------------------------------------------------------------------------------
bool P_CheckPosition(fixed_t px, fixed_t py) {
    if (!cur_map) return true;

    int p_x = FROM_FIXED(px);
    int p_y = FROM_FIXED(py);

    for (int i = 0; i < cur_map->num_lines; i++) {
        const line_t *line = &cur_map->lines[i];

        bool blocks = !(line->flags & ML_TWOSIDED) || (line->backsector == NULL);
        if (!blocks) {
            // Portal step check: block if floor height difference is > 32 units
            int step = FROM_FIXED(line->backsector->floorheight - line->frontsector->floorheight);
            if (step > 32 || step < -64) {
                blocks = true;
            }
        }
        if (!blocks) continue;

        int x1 = FROM_FIXED(line->v1->x);
        int y1 = FROM_FIXED(line->v1->y);
        int x2 = FROM_FIXED(line->v2->x);
        int y2 = FROM_FIXED(line->v2->y);

        int dx = x2 - x1;
        int dy = y2 - y1;
        int len_sq = dx * dx + dy * dy;
        if (len_sq <= 0) continue;

        int dot = (p_x - x1) * dx + (p_y - y1) * dy;
        int close_x, close_y;
        if (dot <= 0) {
            close_x = x1;
            close_y = y1;
        } else if (dot >= len_sq) {
            close_x = x2;
            close_y = y2;
        } else {
            close_x = x1 + (dot * dx) / len_sq;
            close_y = y1 + (dot * dy) / len_sq;
        }

        int dist_x = p_x - close_x;
        int dist_y = p_y - close_y;
        int dist_sq = dist_x * dist_x + dist_y * dist_y;

        // Player collision radius = 18 units (18^2 = 324)
        // Strictly maintains camera distance >= 18 units from walls,
        // completely preventing near-plane clipping (tz = 4) and seeing through walls.
        if (dist_sq < 324) {
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------------------------
// Sector Floor Elevation Tracking
// ------------------------------------------------------------------------------
fixed_t P_GetSectorFloorAt(fixed_t px, fixed_t py) {
    if (cur_map_idx == 0) {
        // MAP 1: E1M1 Hangar & Toxic Courtyard
        if (py >= TO_FIXED(250)) {
            // Courtyard Center Armor Pedestal
            if (px >= TO_FIXED(-50) && px <= TO_FIXED(50) && py >= TO_FIXED(380) && py <= TO_FIXED(480)) {
                return TO_FIXED(32);
            }
            // Sunken Toxic Slime Courtyard
            return TO_FIXED(-32);
        } else if (py >= TO_FIXED(100)) {
            // Elevated Staircase (+32 units)
            return TO_FIXED(32);
        } else if (px >= TO_FIXED(200)) {
            // Computer Terminal Alcove (+16 units)
            return TO_FIXED(16);
        }
        // Starting Starport Hangar
        return TO_FIXED(0);
    } else {
        // MAP 2: MAP02 Underhalls Canals
        if (px <= TO_FIXED(-120)) {
            // Raised Sniper Battlement (+48 units)
            return TO_FIXED(48);
        } else if (py <= TO_FIXED(-120) && px >= TO_FIXED(-50) && px <= TO_FIXED(50)) {
            // Dark Armory Bunker (+16 units)
            return TO_FIXED(16);
        } else if (px >= TO_FIXED(40) && px <= TO_FIXED(120) && py >= TO_FIXED(180) && py <= TO_FIXED(240)) {
            // Obstacle Pillar (+32 units)
            return TO_FIXED(32);
        } else if (px >= TO_FIXED(120) || py >= TO_FIXED(120)) {
            // Sunken Toxic Sludge Canal (-32 units)
            return TO_FIXED(-32);
        }
        // Central Tech Hub (0 units)
        return TO_FIXED(0);
    }
}
