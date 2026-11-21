#ifndef DR_PALETTE_FILTER_H
#define DR_PALETTE_FILTER_H
#include <stdint.h>
#include <limits.h>

#define DR_PALETTE_LUT_SIZE (32 * 32 * 32)

/* A 5-bit/channel RGB cube maps filtered colors back to the existing indexed
 * framebuffer. Palette channel order is shared with the shader (internally BGR).
 * Index zero is reserved for transparent coverage and must never be generated.
 * Rebuild only when an active filtering session's palette actually changes. */
static inline void DRBuildPaletteLUT5(uint8_t *lut, const uint8_t *palette) {
    for (unsigned b = 0; b < 32; ++b) {
        int cb = (int)((b * 255 + 15) / 31);
        for (unsigned g = 0; g < 32; ++g) {
            int cg = (int)((g * 255 + 15) / 31);
            for (unsigned r = 0; r < 32; ++r) {
                int cr = (int)((r * 255 + 15) / 31);
                unsigned best = 1;
                int best_distance = INT_MAX;
                for (unsigned i = 1; i < 256; ++i) {
                    int dr = cr - palette[i * 4];
                    int dg = cg - palette[i * 4 + 1];
                    int db = cb - palette[i * 4 + 2];
                    int distance = dr * dr + dg * dg + db * db;
                    if (distance < best_distance) {
                        best_distance = distance;
                        best = i;
                    }
                }
                lut[r + 32 * g + 1024 * b] = (uint8_t)best;
            }
        }
    }
}

/* Palette fades (SetFadedPalette) scale every colour by one factor. If now is
 * ref scaled by k (0 <= k <= 2, within rounding), return k so the LUT built for
 * ref can be reused by dividing filtered colours by k; return -1 otherwise.
 * Rebuilding the 32768-cell LUT costs ~8M distance tests, and fades set a new
 * palette many times per frame. k > 2 is refused so a LUT built from a very
 * dark palette is replaced as the palette brightens (a few rebuilds at most). */
static inline float DRPaletteFadeScale(const uint8_t *ref, const uint8_t *now) {
    long ref_sum = 0, now_sum = 0;
    for (unsigned i = 1; i < 256; ++i)
        for (unsigned c = 0; c < 3; ++c) {
            ref_sum += ref[i * 4 + c];
            now_sum += now[i * 4 + c];
        }
    if (ref_sum == 0) return now_sum == 0 ? 1.f : -1.f;
    float k = (float)now_sum / (float)ref_sum;
    if (k > 2.f) return -1.f;
    for (unsigned i = 1; i < 256; ++i)
        for (unsigned c = 0; c < 3; ++c) {
            float expected = ref[i * 4 + c] * k;
            float error = now[i * 4 + c] - expected;
            if (error < 0) error = -error;
            if (error > 2.f + expected * (1.f / 64.f)) return -1.f;
        }
    return k;
}
#endif
