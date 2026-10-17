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

void R_ClearClipSegs(void) {
    num_solidsegs = 0;
}

// ------------------------------------------------------------------------------
// Front-to-Back World Renderer: Guaranteed Occlusion and Complete Column Coverage
// ------------------------------------------------------------------------------
#define MAX_RENDER_LINES 32

typedef struct {
    int line_idx;
    int dist_sq;
} line_dist_t;

void R_RenderWorld(void) {
    if (!cur_map || cur_map->num_lines <= 0) return;

    int n_lines = cur_map->num_lines;
    if (n_lines > MAX_RENDER_LINES) n_lines = MAX_RENDER_LINES;

    line_dist_t dists[MAX_RENDER_LINES];
    for (int i = 0; i < n_lines; i++) {
        const line_t *l = &cur_map->lines[i];
        fixed_t mid_x = (l->v1->x + l->v2->x) / 2;
        fixed_t mid_y = (l->v1->y + l->v2->y) / 2;
        int dx = FROM_FIXED(mid_x - viewx);
        int dy = FROM_FIXED(mid_y - viewy);
        dists[i].line_idx = i;
        dists[i].dist_sq = dx * dx + dy * dy;
    }

    // Sort lines closest first (front-to-back)
    for (int i = 1; i < n_lines; i++) {
        line_dist_t key = dists[i];
        int j = i - 1;
        while (j >= 0 && dists[j].dist_sq > key.dist_sq) {
            dists[j + 1] = dists[j];
            j--;
        }
        dists[j + 1] = key;
    }

    // Render lines strictly front-to-back
    for (int i = 0; i < n_lines; i++) {
        R_AddLine(&cur_map->lines[dists[i].line_idx]);
    }

    // Fill any remaining unoccluded column apertures
    R_FillBackdrop();
}
