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
#endif
