#ifndef DR_ANDROID_GRAPHICS_H
#define DR_ANDROID_GRAPHICS_H
#ifdef __ANDROID__
#include <stdlib.h>
#include <string.h>
/* Rendering-only preference: retain the original saved PC settings and AI. */
static inline float DRAndroidDistanceScale(void) {
    const char* setting = getenv("DETHRACE_DRAW_DISTANCE");
    if (setting && strcmp(setting,"far")==0) return 2.f;
    if (setting && strcmp(setting,"extended")==0) return 1.5f;
    return 1.f;
}
static inline float DRAndroidCameraYon(float original) {
    return original * DRAndroidDistanceScale();
}
static inline float DRAndroidSceneryFactor(float original) {
    /* A reduced PC scenery factor can discard columns before the far plane. */
    return DRAndroidDistanceScale()>1.f ? 1.f : original;
}
#endif
#endif
