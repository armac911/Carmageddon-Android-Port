#ifndef DR_ANDROID_EFFECTS_H
#define DR_ANDROID_EFFECTS_H
/* Optional enhanced effects (Android launcher options, off by default).
 * They extend the game's own skid, smoke, spark and shrapnel systems, so
 * replays, depth cueing and the renderer treat them like original effects.
 *
 * Pools are sized for the enhanced mode; with the option off only the
 * original number of entries is ever used. Smoke and shrapnel are tracked in
 * 32-bit flag masks (so 32 is their hard maximum); replays store skid indices
 * as 16 bits. */
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#define DR_SKID_POOL 300
#define DR_SMOKE_POOL 32
#define DR_SHRAPNEL_POOL 32

static inline int DRAndroidEffectSetting(const char* name, int* cache) {
    if (*cache < 0) {
        const char* value = getenv(name);
        *cache = value && strcmp(value, "on") == 0;
    }
    return *cache;
}
/* DETHRACE_FX_TYRES=on: steady tyre smoke when sliding, longer skid trails. */
static inline int DRAndroidTyreEffects(void) {
    static int cache = -1;
    return DRAndroidEffectSetting("DETHRACE_FX_TYRES", &cache);
}
/* DETHRACE_FX_DEBRIS=on: more sparks and car fragments on impacts. */
static inline int DRAndroidDebrisEffects(void) {
    static int cache = -1;
    return DRAndroidEffectSetting("DETHRACE_FX_DEBRIS", &cache);
}
#else
#define DR_SKID_POOL 100
#define DR_SMOKE_POOL 25
#define DR_SHRAPNEL_POOL 15
static inline int DRAndroidTyreEffects(void) { return 0; }
static inline int DRAndroidDebrisEffects(void) { return 0; }
#endif

/* Entries in use: the original counts unless the option is on. */
static inline int DRSkidLimit(void) { return DRAndroidTyreEffects() ? DR_SKID_POOL : 100; }
static inline int DRSmokeLimit(void) { return DRAndroidTyreEffects() ? DR_SMOKE_POOL : 25; }
static inline int DRShrapnelLimit(void) { return DRAndroidDebrisEffects() ? DR_SHRAPNEL_POOL : 15; }

/* Enhanced tyre smoke for a wheel sliding at pV (the game's skid speed, whose
 * original puff strength is pV / 25): denser and longer lived. */
static inline float DRTyreSmokeStrength(float pV) {
    float strength = pV / 12.f + 0.15f;
    return strength < 0.25f ? 0.25f : strength > 0.9f ? 0.9f : strength;
}
#define DR_TYRE_SMOKE_DECAY 0.6f       /* strength lost per second (original: 1.0) */
#define DR_TYRE_SMOKE_INTERVAL_MS 70u  /* per wheel, so the smoke pool keeps a trail */
#define DR_TYRE_SMOKE_MIN_SPEED 2.f    /* no smoke for gentle slip */

/* Spark and fragment counts for the enhanced mode. */
static inline int DRSparkCount(int original, int original_cap) {
    if (!DRAndroidDebrisEffects()) return original > original_cap ? original_cap : original;
    int count = original + original / 2;
    int cap = original > 16 ? original : 16; /* never fewer than the original */
    return count > cap ? cap : count;
}
static inline int DRShrapnelCount(int original) {
    if (!DRAndroidDebrisEffects()) return original;
    int count = original * 2;
    return count > DR_SHRAPNEL_POOL ? DR_SHRAPNEL_POOL : count;
}
#endif
