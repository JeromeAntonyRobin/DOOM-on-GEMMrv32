// ==============================================================================
// GEMMrv32 DOOM: Multi-Map Level Engine (E1M1 Hangar & MAP02 Underhalls Canals)
// Fixed-Point 16.16 Coordinates with Variable Heights (Stairs, Slime Pit, Portals)
// ==============================================================================

#ifndef DOOM_MAP_H
#define DOOM_MAP_H

#include "doom_defs.h"

typedef struct {
    fixed_t x, y, z;
    mobjtype_t type;
    angle_t angle;
} mobj_spawn_t;

typedef struct {
    const char *name;
    const char *title;
    const vertex_t *vertices;
    int num_vertices;
    const sector_t *sectors;
    int num_sectors;
    const line_t *lines;
    int num_lines;
    const mobj_spawn_t *spawns;
    int num_spawns;
    fixed_t start_x, start_y, start_z;
    int start_angle_idx;
} doom_map_t;

#define NUM_MAPS 2

extern const doom_map_t maps[NUM_MAPS];
extern const doom_map_t *cur_map;
extern int cur_map_idx;

void    P_InitMaps(void);
void    P_LoadMap(int map_idx);
bool    P_CheckPosition(fixed_t px, fixed_t py);
fixed_t P_GetSectorFloorAt(fixed_t px, fixed_t py);

#endif // DOOM_MAP_H
