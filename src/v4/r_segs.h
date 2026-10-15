#ifndef R_SEGS_H
#define R_SEGS_H

#include "doom_defs.h"

// ------------------------------------------------------------------------------
// Linedef Projection, Wall Rendering & Clipping Functions
// ------------------------------------------------------------------------------

void R_InitSegs(void);
void R_ClearDrawSegs(void);
void R_AddLine(line_t *line);
void R_StoreWallRange(int x1, int x2, line_t *line, fixed_t tz1, fixed_t tz2);
void R_FillBackdrop(void);

// 1D Screen Column Clipping Buffers (Top ceiling clip, Bottom floor clip)
extern int16_t ceilingclip[SCREEN_W];
extern int16_t floorclip[SCREEN_W];

// Depth buffer for 320 columns
extern fixed_t wall_dist_buffer[SCREEN_W];

#endif // R_SEGS_H
