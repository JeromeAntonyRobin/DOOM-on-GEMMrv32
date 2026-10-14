#ifndef P_MOBJ_H
#define P_MOBJ_H

#include "doom_defs.h"

// ------------------------------------------------------------------------------
// Map Object Functions (Spawn, Thinkers, AI, Projectiles, Sprites)
// ------------------------------------------------------------------------------

void P_InitMobjs(void);
mobj_t* P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type, angle_t angle);
void P_RemoveMobj(mobj_t *mobj);
void P_MobjThinker(mobj_t *mobj);
void P_Ticker(void);
void R_DrawMasked(void);

extern mobj_t *player_mobj;

#endif // P_MOBJ_H
