#ifndef DR_ANDROID_GRAPHICS_H
#define DR_ANDROID_GRAPHICS_H
#ifdef __ANDROID__
#include <stdlib.h>
#include <string.h>
/* Rendering-only preference: retain the original saved PC settings and AI. */
static inline float DRAndroidDistanceScale(void) {
    static float cached = 0.f; /* read per frame by the track renderer */
    if (cached == 0.f) {
        const char* setting = getenv("DETHRACE_DRAW_DISTANCE");
        cached = setting && strcmp(setting,"far")==0 ? 2.f
            : setting && strcmp(setting,"extended")==0 ? 1.5f : 1.f;
    }
    return cached;
}
static inline float DRAndroidCameraYon(float original) {
    return original * DRAndroidDistanceScale();
}
static inline float DRAndroidSceneryFactor(float original) {
    /* A reduced PC scenery factor can discard columns before the far plane. */
    return DRAndroidDistanceScale()>1.f ? 1.f : original;
}
/* DETHRACE_TRACK_CULL=off restores 0.9.2's track column selection (the
 * box around the view) without the per-column view test. */
static inline int DRAndroidViewCulling(void) {
    static int cached = -1; /* launcher settings are fixed for a game session */
    if (cached < 0) {
        const char* setting = getenv("DETHRACE_TRACK_CULL");
        cached = !(setting && strcmp(setting,"off")==0);
    }
    return cached;
}
/* DETHRACE_MIRROR: cockpit rear-view mirror cost control.
 * 0 = every frame, 1 = every second frame (the previous image is reused),
 * 2 = mirror starts switched off (the in-game mirror key still toggles it). */
static inline int DRAndroidMirrorMode(void) {
    static int cached = -1; /* read every frame: cache the launcher setting */
    if (cached < 0) {
        const char* setting = getenv("DETHRACE_MIRROR");
        cached = setting && strcmp(setting,"half")==0 ? 1 : setting && strcmp(setting,"off")==0 ? 2 : 0;
    }
    return cached;
}
/* DETHRACE_FOV widens the external (chase) camera only. The cockpit camera's
 * angle is derived from the cockpit artwork's window and must stay aligned
 * with it, and widening the rear-view mirror would add cost for no benefit
 * (both keep gCamera_angle from GENERAL.TXT). Capped at 120 degrees. */
static inline float DRAndroidExternalCameraAngle(float original) {
    const char* setting = getenv("DETHRACE_FOV");
    float extra = 0.f;
    if (setting && strcmp(setting,"wide")==0) extra = 8.f;
    else if (setting && strcmp(setting,"wider")==0) extra = 16.f;
    float angle = original + extra;
    return angle > 120.f ? (original > 120.f ? original : 120.f) : angle;
}
#endif
#endif
