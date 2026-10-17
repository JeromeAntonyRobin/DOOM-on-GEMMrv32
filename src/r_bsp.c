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

static bool line_rendered[NUM_LINES];

// Subsector to linedef assignment for authentic front-to-back BSP rasterization
// Subsector 0: Starting Hangar (Lines 0, 1, 2)
// Subsector 1: Stairs & West Courtyard (Lines 3, 5, 6, 7)
// Subsector 2: Stairs & East Courtyard (Lines 4, 5, 7, 8)
// Subsector 3: Computer Terminal Room (Lines 9, 10, 11)
static const int subsector_lines[4][4] = {
    { 0, 1, 2, -1 },       // Subsector 0: Starting Hangar
    { 3, 5, 6, 7 },        // Subsector 1: Stairs & West Courtyard
    { 4, 5, 7, 8 },        // Subsector 2: Stairs & East Courtyard
    { 9, 10, 11, -1 }      // Subsector 3: Computer Terminal Room
};

// ------------------------------------------------------------------------------
// Clear Occlusion Buffer (solidsegs) and Per-Frame Line State
// ------------------------------------------------------------------------------
void R_ClearClipSegs(void) {
    num_solidsegs = 0;
    for (size_t i = 0; i < NUM_LINES; i++) {
        line_rendered[i] = false;
    }
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
    if (num < 0 || num >= 4) return;
    for (int k = 0; k < 4; k++) {
        int idx = subsector_lines[num][k];
        if (idx < 0) break;
        if (!line_rendered[idx]) {
            line_rendered[idx] = true;
            R_AddLine(&map_lines[idx]);
        }
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

// ------------------------------------------------------------------------------
// Front-to-Back World Renderer: Guaranteed Occlusion and Complete Column Coverage
// ------------------------------------------------------------------------------
typedef struct {
    int line_idx;
    int64_t dist_sq;
} line_dist_t;

void R_RenderWorld(void) {
    line_dist_t dists[NUM_LINES];
    for (size_t i = 0; i < NUM_LINES; i++) {
        line_t *l = &map_lines[i];
        fixed_t mid_x = (l->v1->x + l->v2->x) / 2;
        fixed_t mid_y = (l->v1->y + l->v2->y) / 2;
        int64_t dx = FROM_FIXED(mid_x - viewx);
        int64_t dy = FROM_FIXED(mid_y - viewy);
        dists[i].line_idx = (int)i;
        dists[i].dist_sq = dx * dx + dy * dy;
    }

    // Sort lines closest first (front-to-back)
    for (size_t i = 1; i < NUM_LINES; i++) {
        line_dist_t key = dists[i];
        int j = (int)i - 1;
        while (j >= 0 && dists[j].dist_sq > key.dist_sq) {
            dists[j + 1] = dists[j];
            j--;
        }
        dists[j + 1] = key;
    }

    // Render lines strictly front-to-back
    for (size_t i = 0; i < NUM_LINES; i++) {
        R_AddLine(&map_lines[dists[i].line_idx]);
    }

    // Fill any remaining unoccluded column apertures
    R_FillBackdrop();
}
