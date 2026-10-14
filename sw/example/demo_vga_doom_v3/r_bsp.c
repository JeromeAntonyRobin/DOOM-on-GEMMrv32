#include "r_bsp.h"
#include "doom_map.h"
#include "r_segs.h"

cliprange_t solidsegs[MAX_SOLIDSEGS];
int num_solidsegs = 0;

fixed_t viewx = 0;
fixed_t viewy = 0;
fixed_t viewz = 0;
angle_t viewangle = 0;
fixed_t viewcos = FP_ONE;
fixed_t viewsin = 0;

// ------------------------------------------------------------------------------
// Clear Occlusion Buffer (solidsegs)
// ------------------------------------------------------------------------------
void R_ClearClipSegs(void) {
    num_solidsegs = 0;
}

// ------------------------------------------------------------------------------
// Evaluate Point Side relative to Node Partition Line
// ------------------------------------------------------------------------------
int R_PointOnSide(fixed_t x, fixed_t y, const node_t *node) {
    if (!node->dx) {
        if (x <= node->x) return node->dy > 0;
        return node->dy < 0;
    }
    if (!node->dy) {
        if (y <= node->y) return node->dx < 0;
        return node->dx > 0;
    }

    fixed_t dx = (x - node->x);
    fixed_t dy = (y - node->y);

    int64_t left  = (int64_t)node->dy * (int64_t)dx;
    int64_t right = (int64_t)dy * (int64_t)node->dx;

    if (right < left) return 0; // Front side
    return 1;                   // Back side
}

// ------------------------------------------------------------------------------
// Check if Bounding Box is completely occluded by solidsegs
// ------------------------------------------------------------------------------
bool R_CheckBBox(const fixed_t bspcoord[4]) {
    (void)bspcoord;
    // If screen is 100% full, early exit
    if (num_solidsegs == 1 && solidsegs[0].first == 0 && solidsegs[0].last >= SCREEN_W - 1) {
        return false; // Occluded!
    }
    return true; // Partially visible, continue traversal
}

// ------------------------------------------------------------------------------
// Subsector Processing (Linedef Wall Rendering)
// ------------------------------------------------------------------------------
void R_Subsector(int num) {
    (void)num;
    // Loop over linedefs in map and render lines belonging to this subsector
    for (size_t i = 0; i < NUM_LINES; i++) {
        line_t *line = &map_lines[i];
        R_AddLine(line);
    }
}

// ------------------------------------------------------------------------------
// Recursive 2D BSP Tree Traversal
// ------------------------------------------------------------------------------
void R_RenderBSPNode(int bspnum) {
    // Base Case: Subsector reached
    if (bspnum & NF_SUBSECTOR) {
        R_Subsector(bspnum & ~NF_SUBSECTOR);
        return;
    }

    const node_t *node = &map_nodes[bspnum];

    // Determine front/back order based on player position
    int side = R_PointOnSide(viewx, viewy, node);

    // 1. Traverse closer geometry first (front-to-back)
    R_RenderBSPNode(node->children[side]);

    // 2. Check if further geometry is occluded before traversing
    if (R_CheckBBox(node->bbox[side ^ 1])) {
        R_RenderBSPNode(node->children[side ^ 1]);
    }
}
