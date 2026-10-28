/* Android input response only; vehicle grip and physics remain original. */
#ifndef DR_ANDROID_CONTROL_TUNING_H
#define DR_ANDROID_CONTROL_TUNING_H
#include <stdlib.h>
#include <math.h>
typedef struct {
    int stick_deadzone, stick_sensitivity, touch_deadzone, touch_sensitivity;
    int trigger_deadzone, trigger_sensitivity, action_deadzone;
    int dpad_sensitivity, steer_rise, steer_return, pedal_rise;
} DRControlTuning;
static inline int dr_control_setting(const char* key, int fallback, int lo, int hi) {
    const char* s = getenv(key);
    if (!s || !*s) return fallback;
    char* end;
    long value = strtol(s, &end, 10);
    if (*end || value < lo || value > hi) return fallback;
    return (int)value;
}
static inline DRControlTuning* dr_control_tuning(int reset) {
    static DRControlTuning c;
    static int loaded;
    if (!loaded || reset) {
        loaded = 1;
#define LOAD(field, key, def, lo, hi) c.field = dr_control_setting("DETHRACE_INPUT_" key, def, lo, hi)
        LOAD(stick_deadzone, "STICK_DEADZONE", 24, 0, 50);
        LOAD(stick_sensitivity, "STICK_SENSITIVITY", 85, 25, 150);
        LOAD(touch_deadzone, "TOUCH_DEADZONE", 8, 0, 50);
        LOAD(touch_sensitivity, "TOUCH_SENSITIVITY", 85, 25, 150);
        LOAD(trigger_deadzone, "TRIGGER_DEADZONE", 10, 0, 50);
        LOAD(trigger_sensitivity, "TRIGGER_SENSITIVITY", 100, 25, 150);
        LOAD(action_deadzone, "ACTION_DEADZONE", 50, 20, 90);
        LOAD(dpad_sensitivity, "DPAD_SENSITIVITY", 85, 25, 100);
        LOAD(steer_rise, "STEER_RISE", 250, 0, 1000);
        LOAD(steer_return, "STEER_RETURN", 180, 0, 1000);
        LOAD(pedal_rise, "PEDAL_RISE", 120, 0, 500);
#undef LOAD
    }
    return &c;
}
/* A lower sensitivity softens the centre while preserving full analog travel.
 * The engine already squares steering, so this is applied before that curve. */
static inline int dr_control_axis(int value, int maximum, int deadzone, int sensitivity) {
    if (value > maximum) value = maximum;
    if (value < -maximum) value = -maximum;
    float x = (float)abs(value) / maximum;
    float dz = deadzone / 100.f;
    if (x <= dz) return 0;
    x = (x - dz) / (1.f - dz);
    x = powf(x, 100.f / sensitivity);
    int scaled = (int)(x * 65535.f + .5f);
    return value < 0 ? -scaled : scaled;
}
/* Linear rate limit with time split at zero for frame-rate independent reversal.
 * Full release of a pedal uses return_ms=0, so brakes never stick on release. */
static inline float dr_control_slew(float current, int target, unsigned dt, int rise_ms, int return_ms) {
    float time = (float)dt;
    if (current * target < 0.f) {
        float to_zero = fabsf(current) * return_ms / 65535.f;
        if (time < to_zero) return current + (current > 0 ? -1.f : 1.f) * time * 65535.f / return_ms;
        time -= to_zero;
        current = 0.f;
    }
    int duration = fabsf((float)target) < fabsf(current) ? return_ms : rise_ms;
    float step = duration ? time * 65535.f / duration : 131070.f;
    if (fabsf(target - current) <= step) return (float)target;
    return current + (target > current ? step : -step);
}
#endif
