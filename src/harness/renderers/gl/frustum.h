#ifndef DR_GL_FRUSTUM_H
#define DR_GL_FRUSTUM_H
#include <math.h>
/* Matrices use the same column-major memory interpretation as GLES uniforms.
 * Extract world-space planes from projection * view; inside is >= 0. */
static inline void DRFrustumPlanes(float planes[6][4], const float view[4][4], const float projection[4][4]) {
    for (int axis = 0; axis < 3; ++axis) {
        for (int side = 0; side < 2; ++side) {
            float sign = side ? -1.f : 1.f;
            for (int column = 0; column < 4; ++column) {
                double value = 0;
                for (int k = 0; k < 4; ++k)
                    value += ((double)projection[k][3] + sign * projection[k][axis]) * view[column][k];
                planes[axis * 2 + side][column] = (float)value;
            }
        }
    }
}
/* Reject only if the entire transformed box lies outside one clip plane.
 * No perspective division: boxes crossing the eye/near plane remain safe.
 * Invalid bounds/transforms fail open rather than hiding game geometry. */
static inline int DRBoundsVisible(const float minimum[3], const float maximum[3], const float model[4][3], const float planes[6][4]) {
    for (int a = 0; a < 3; ++a) {
        if (!isfinite(minimum[a]) || !isfinite(maximum[a]) || minimum[a] > maximum[a]) return 1;
        for (int row = 0; row < 4; ++row) if (!isfinite(model[row][a])) return 1;
    }
    for (int p = 0; p < 6; ++p)
        for (int a = 0; a < 4; ++a) if (!isfinite(planes[p][a])) return 1;
    for (int p = 0; p < 6; ++p) {
        double distance = planes[p][3], magnitude = fabs(distance);
        for (int a = 0; a < 3; ++a) {
            double term = (double)planes[p][a] * model[3][a];
            distance += term; magnitude += fabs(term);
        }
        for (int a = 0; a < 3; ++a) {
            double coefficient = 0;
            for (int k = 0; k < 3; ++k) coefficient += (double)planes[p][k] * model[a][k];
            double term = coefficient * (coefficient >= 0 ? maximum[a] : minimum[a]);
            distance += term; magnitude += fabs(term);
        }
        /* Keep edge-touching boxes and allow for CPU/GPU float rounding, including mediump shaders. */
        if (distance < -2e-3 * (1.0 + magnitude)) return 0;
    }
    return 1;
}
#endif
