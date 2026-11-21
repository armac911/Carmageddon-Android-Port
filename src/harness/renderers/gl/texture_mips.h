#ifndef DR_TEXTURE_MIPS_H
#define DR_TEXTURE_MIPS_H
#include <stdint.h>


/* Mip chains for indexed (8-bit palette) material textures.
 *
 * Integer textures cannot be averaged by the GPU, and the 3D shader still has
 * to apply the material shade table to each texel index. So every mip texel is
 * an ORIGINAL index from level 0: the opaque texel of its source block whose
 * colour is closest to the block's average opaque colour (a medoid). This
 * keeps shading exact, never invents colours, and the choice does not change
 * when the palette is faded (scaled) towards black.
 *
 * Index 0 is transparent. A mip texel is 0 only when its whole source block is
 * transparent; cutout coverage itself is still decided from level 0.
 *
 * palette is RGBA (any channel order, alpha ignored) or NULL when no usable
 * palette is known yet; then the first opaque texel of each block is used. */

static inline int DRMipLevelCount(int width, int height) {
    int levels = 1;
    while (width > 1 || height > 1) {
        width = width > 1 ? width / 2 : 1;
        height = height > 1 ? height / 2 : 1;
        ++levels;
    }
    return levels;
}

/* Build one mip level (out_w x out_h) straight from level 0 (src_w x src_h).
 * Each output texel covers the level-0 block [x*src_w/out_w, (x+1)*src_w/out_w)
 * on each axis, which also handles non-power-of-two sizes. */
static inline void DRBuildIndexedMip(uint8_t* out, int out_w, int out_h,
                                     const uint8_t* src, int src_w, int src_h,
                                     const uint8_t* palette) {
    for (int y = 0; y < out_h; ++y) {
        int y0 = (int)((long)y * src_h / out_h);
        int y1 = (int)((long)(y + 1) * src_h / out_h);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < out_w; ++x) {
            int x0 = (int)((long)x * src_w / out_w);
            int x1 = (int)((long)(x + 1) * src_w / out_w);
            if (x1 <= x0) x1 = x0 + 1;

            int64_t sum[3] = { 0, 0, 0 };
            int64_t count = 0;
            uint8_t first = 0;
            for (int sy = y0; sy < y1; ++sy) {
                for (int sx = x0; sx < x1; ++sx) {
                    uint8_t index = src[sy * src_w + sx];
                    if (index == 0) continue;
                    if (count == 0) first = index;
                    ++count;
                    if (palette) {
                        sum[0] += palette[index * 4];
                        sum[1] += palette[index * 4 + 1];
                        sum[2] += palette[index * 4 + 2];
                    }
                }
            }
            uint8_t result = first;
            if (palette && count > 1) {
                /* Compare in count-scaled units to stay in integers. */
                int64_t best_distance = INT64_MAX;
                for (int sy = y0; sy < y1; ++sy) {
                    for (int sx = x0; sx < x1; ++sx) {
                        uint8_t index = src[sy * src_w + sx];
                        if (index == 0) continue;
                        int64_t distance = 0;
                        for (int k = 0; k < 3; ++k) {
                            int64_t d = palette[index * 4 + k] * count - sum[k];
                            distance += d * d;
                        }
                        if (distance < best_distance) {
                            best_distance = distance;
                            result = index;
                        }
                    }
                }
            }
            out[y * out_w + x] = result;
        }
    }
}

/* Palettes this dark (fades, loading screens) cannot rank colours reliably. */
static inline int DRPaletteUsableForMips(const uint8_t* palette) {
    int64_t total = 0;
    for (int i = 1; i < 256; ++i)
        total += palette[i * 4] + palette[i * 4 + 1] + palette[i * 4 + 2];
    return total >= (int64_t)255 * 3 * 16; /* average channel >= 16 */
}
#endif
