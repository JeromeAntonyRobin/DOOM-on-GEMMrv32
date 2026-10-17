#ifndef R_BSP_H
#define R_BSP_H

#include "doom_defs.h"

// ------------------------------------------------------------------------------
// BSP Traversal & SolidSeg Occlusion Functions
// ------------------------------------------------------------------------------

void R_ClearClipSegs(void);
int  R_PointOnSide(fixed_t x, fixed_t y, const node_t *node);
bool R_CheckBBox(const fixed_t bspcoord[4]);
void R_RenderBSPNode(int bspnum);
void R_Subsector(int num);
void R_RenderWorld(void);

// View Transformation Context
extern fixed_t viewx, viewy, viewz;
extern angle_t viewangle;
extern fixed_t viewcos, viewsin;

#endif // R_BSP_H
