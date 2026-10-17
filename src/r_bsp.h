#ifndef R_BSP_H
#define R_BSP_H

#include "doom_defs.h"

// ------------------------------------------------------------------------------
// Front-to-Back Line Renderer & Occlusion
// ------------------------------------------------------------------------------
void R_ClearClipSegs(void);
void R_RenderWorld(void);

// View Transformation Context
extern fixed_t viewx, viewy, viewz;
extern angle_t viewangle;
extern fixed_t viewcos, viewsin;

#endif // R_BSP_H
