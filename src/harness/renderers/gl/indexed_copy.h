#ifndef DR_INDEXED_COPY_H
#define DR_INDEXED_COPY_H
#include <stdint.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
/* Readback storage and CPU pixmap never overlap. Zero is transparent. */
static inline void DRMergeIndexedRow(uint8_t* restrict dst, const uint8_t* restrict src, int width) {
    int x = 0;
#if defined(__aarch64__)
    const uint8x16_t zero = vdupq_n_u8(0);
    for (; x + 16 <= width; x += 16) {
        uint8x16_t pixels = vld1q_u8(src + x);
        uint8x16_t old = vld1q_u8(dst + x);
        vst1q_u8(dst + x, vbslq_u8(vceqq_u8(pixels, zero), old, pixels));
    }
#endif
    for (; x < width; ++x) dst[x] = src[x] ? src[x] : dst[x];
}
#endif
