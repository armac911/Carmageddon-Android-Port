#ifndef DR_COLUMN_CULL_H
#define DR_COLUMN_CULL_H
#include <math.h>

/* Track columns are picked by RenderTrack as the axis-aligned box around the
 * camera and the far corners of the view. With wide aspect ratios and wider
 * fields of view that box is several times the area actually in view, and
 * every column in it is traversed, transformed and submitted (lollipops are
 * re-oriented on the CPU too). This adds a conservative 2D test: a column is
 * skipped only when its whole XZ rectangle lies outside a horizontal wedge
 * that provably contains the real view for any roll and pitch (plus a margin, with
 * the apex pulled back by a column so nearby geometry is always kept).
 * The test switches itself off whenever the wedge would not help or might be
 * wrong (steep pitch, near-180 degree views, degenerate matrices). */

typedef struct tDR_column_wedge {
    int enabled;
    float apex_x, apex_z;   /* wedge apex (camera pulled back) */
    float fwd_x, fwd_z;     /* unit horizontal forward */
    float sin_h, cos_h;     /* half-angle of the wedge */
} tDR_column_wedge;

#define DR_WEDGE_MARGIN_RADIANS 0.14f /* ~8 degrees */
#define DR_WEDGE_MAX_HALF_RADIANS 1.48f /* ~85 degrees: beyond this, no benefit */

/* camera_to_world rows: [0..2] axes, [3] position (BRender br_matrix34).
 * The camera looks down its -Z axis. vertical_fov in radians. */
static inline void DRColumnWedgeSetup(tDR_column_wedge* w, const float camera_to_world[4][3],
    float vertical_fov, float aspect, float column_size) {
    w->enabled = 0;
    float fx = -camera_to_world[2][0], fy = -camera_to_world[2][1], fz = -camera_to_world[2][2];
    float length = sqrtf(fx * fx + fy * fy + fz * fz);
    float horizontal = sqrtf(fx * fx + fz * fz);
    if (!(length > 1e-6f) || !(horizontal > 1e-6f) || !(column_size > 0.f)) return;
    float cos_pitch = horizontal / length;
    float sin_pitch = fabsf(fy) / length;
    if (cos_pitch < 0.5f) return; /* looking steeply up/down (e.g. map views) */
    if (!(vertical_fov > 0.f) || !(aspect > 0.f)) return;
    /* Bound for any camera roll and pitch. A ray in the view is
     * forward + a*right + b*up with sqrt(a^2 + b^2) <= R, the view's diagonal
     * slope. Because the camera axes are orthonormal, its horizontal component
     * along the heading is at least cos(pitch) - R*sin(pitch) and its sideways
     * component at most R, so its heading angle is at most
     * atan(R / (cos(pitch) - R*sin(pitch))). */
    float R = sqrtf(aspect * aspect + 1.f) * tanf(vertical_fov * 0.5f);
    float forward = cos_pitch - R * sin_pitch;
    if (!(forward > 0.05f)) return; /* the view reaches straight up/down */
    float half = atanf(R / forward) + DR_WEDGE_MARGIN_RADIANS;
    if (!(half < DR_WEDGE_MAX_HALF_RADIANS)) return;
    w->fwd_x = fx / horizontal;
    w->fwd_z = fz / horizontal;
    w->apex_x = camera_to_world[3][0] - w->fwd_x * column_size;
    w->apex_z = camera_to_world[3][2] - w->fwd_z * column_size;
    w->sin_h = sinf(half);
    w->cos_h = cosf(half);
    w->enabled = isfinite(w->apex_x) && isfinite(w->apex_z);
}

/* 1 if the rectangle [x0,x1] x [z0,z1] may be visible. The wedge is the
 * intersection of two half-planes through the apex:
 *   sin(h) * dot(F, v) +/- cos(h) * cross(F, v) >= 0,  v = point - apex. */
static inline int DRColumnInWedge(const tDR_column_wedge* w, float x0, float z0, float x1, float z1) {
    if (!w->enabled) return 1;
    const float xs[2] = { x0, x1 }, zs[2] = { z0, z1 };
    for (int side = 0; side < 2; ++side) {
        float sign = side ? -1.f : 1.f;
        int any_inside = 0;
        for (int i = 0; i < 4 && !any_inside; ++i) {
            float vx = xs[i & 1] - w->apex_x, vz = zs[i >> 1] - w->apex_z;
            float dot = w->fwd_x * vx + w->fwd_z * vz;
            float cross = w->fwd_x * vz - w->fwd_z * vx;
            if (w->sin_h * dot + sign * w->cos_h * cross >= 0.f) any_inside = 1;
        }
        if (!any_inside) return 0;
    }
    return 1;
}
#endif
