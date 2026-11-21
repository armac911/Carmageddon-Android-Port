#ifdef __ANDROID__
#include <jni.h>
#include <SDL.h>
#include <SDL_system.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "android_input.h"
#include "harness/profiler.h"
JNIEXPORT void JNICALL Java_com_dethrace_android_DethraceActivity_nativeProfilePause(JNIEnv* env,jobject obj) { DRProfileMarkPause(); }
#include "common/globvars.h"
#include "common/input.h"
#include "constants.h"

static SDL_atomic_t touch_mask, touch_edges, touch_axis, touch_axis_active, touch_reset;
/* Impact strength 0..1000 waiting for the phone vibrator / controller rumble. */
static SDL_atomic_t impact, controller_rumble;
/* Tilt steering from the Java rotation sensor (-65535..65535). */
static SDL_atomic_t tilt_axis, tilt_active;
/* 1: tilt is added to a controller's steering (phone in a controller grip);
 * 0: a controller in use overrides tilt. Set once from DETHRACE_TILT_CONTROLLER. */
static int tilt_with_controller = 1;
static int digital_steering = -1;
static SDL_atomic_t using_gamepad, last_screen_touch;
void DRAndroidGamepadActivity(unsigned timestamp) {
    Uint32 touch = (Uint32)SDL_AtomicGet(&last_screen_touch);
    /* Reject queued controller events older than the most recent screen tap. */
    if (!touch || (Sint32)(timestamp-touch)>0) SDL_AtomicSet(&using_gamepad,1);
}
JNIEXPORT jboolean JNICALL
Java_com_dethrace_android_DethraceActivity_nativeUsingGamepad(JNIEnv* env,jobject obj) {
    return SDL_AtomicGet(&using_gamepad)!=0;
}
JNIEXPORT void JNICALL
Java_com_dethrace_android_DethraceActivity_nativeScreenTouched(JNIEnv* env,jobject obj) {
    SDL_AtomicSet(&last_screen_touch,(int)SDL_GetTicks());
    SDL_AtomicSet(&using_gamepad,0);
}
static int gamepad_steering = 100000; /* Only accessed by the game thread. */
static SDL_atomic_t measured_fps, fps_sample_time;
static Uint32 frame_window, previous_frame;
static unsigned frame_count;
void DRAndroidFramePresented(void) {
    Uint32 now = SDL_GetTicks();
    /* Restart after suspension; never average a background pause into FPS. */
    if (!previous_frame || now - previous_frame > 2000) {
        frame_window = now; frame_count = 0;
        SDL_AtomicSet(&measured_fps, 0);
    } else {
        ++frame_count;
        Uint32 elapsed = now - frame_window;
        if (elapsed >= 1000) {
            SDL_AtomicSet(&measured_fps, (int)((frame_count * 10000u + elapsed/2) / elapsed));
            SDL_AtomicSet(&fps_sample_time, (int)now);
            frame_window = now; frame_count = 0;
        }
    }
    previous_frame = now;
}
JNIEXPORT jint JNICALL
Java_com_dethrace_android_DethraceActivity_nativeFpsTenths(JNIEnv* env, jobject obj) {
    return SDL_GetTicks() - (Uint32)SDL_AtomicGet(&fps_sample_time) > 2000 ? 0 : SDL_AtomicGet(&measured_fps);
}
int DRAndroidDigitalSteering(void) {
    /* Read every frame: cache the launcher setting (fixed for the session). */
    if (digital_steering < 0) {
        const char* mode = getenv("DETHRACE_STEERING");
        digital_steering = mode && strcmp(mode, "digital") == 0;
    }
    return digital_steering;
}
int DRAndroidNormalizeStick(int value) {
    if (value < -32767) value = -32767;
    if (value > 32767) value = 32767;
    int magnitude = abs(value);
    if (magnitude <= 8000) return 0;
    int scaled = (magnitude - 8000) * 65535 / (32767 - 8000);
    return value < 0 ? -scaled : scaled;
}
void DRAndroidSetGamepadSteering(int value) { gamepad_steering = value; }
/* Same action bits as touch; both paths are combined on the game thread. */
static unsigned gamepad_mask;
static int pending_gear_change;
void DRAndroidSetGamepadButtons(unsigned mask) {
    unsigned edges = mask & ~gamepad_mask;
    gamepad_mask = mask & (127u | 256u | 512u);
    if (!gProgram_state.racing || gInterface_within_race_mode || !(mask & (256u | 512u))) pending_gear_change = 0;
    else if (edges & (256u | 512u))
        pending_gear_change = (edges & 256u) ? 1 : -1;
}
int DRAndroidTakeGearChange(void) {
    int change = pending_gear_change;
    pending_gear_change = 0;
    return change;
}
static unsigned active_mask;
static Uint32 held_until[8];
static SDL_SpinLock text_lock;
static int text_session, text_pending, text_limit;
static char text_reply[101];

JNIEXPORT jboolean JNICALL
Java_com_dethrace_android_DethraceActivity_nativeIsInRace(JNIEnv* env, jobject obj) {
    return gProgram_state.racing && !gInterface_within_race_mode;
}
JNIEXPORT void JNICALL
Java_com_dethrace_android_DethraceActivity_nativeTouch(JNIEnv* env, jobject obj, jint mask, jint axis, jboolean steering) {
    int old = SDL_AtomicSet(&touch_mask, mask & 255);
    int edges = (mask & ~old) & 255, previous;
    do { previous = SDL_AtomicGet(&touch_edges); }
    while (!SDL_AtomicCAS(&touch_edges, previous, previous | edges));
    SDL_AtomicSet(&touch_axis, axis < -65535 ? -65535 : axis > 65535 ? 65535 : axis);
    SDL_AtomicSet(&touch_axis_active, steering);
}
JNIEXPORT void JNICALL
Java_com_dethrace_android_DethraceActivity_nativeResetTouch(JNIEnv* env, jobject obj) {
    SDL_AtomicSet(&touch_reset, 1);
    SDL_AtomicSet(&touch_mask, 0);
    SDL_AtomicSet(&touch_edges, 0);
    SDL_AtomicSet(&touch_axis_active, 0);
}
void DRAndroidUpdateTouch(void) {
    if (SDL_AtomicSet(&touch_reset, 0)) memset(held_until, 0, sizeof(held_until));
    unsigned edges = SDL_AtomicSet(&touch_edges, 0);
    unsigned down = SDL_AtomicGet(&touch_mask);
    Uint32 now = SDL_GetTicks();
    active_mask = 0;
    if (!gProgram_state.racing || gInterface_within_race_mode) {
        memset(held_until, 0, sizeof(held_until));
        SDL_AtomicSet(&touch_mask, 0);
        SDL_AtomicSet(&touch_axis_active, 0);
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (edges & (1u << i)) held_until[i] = now + 80;
        if ((down & (1u << i)) || (held_until[i] && (Sint32)(held_until[i] - now) > 0))
            active_mask |= 1u << i;
    }
}
int DRAndroidTouchKey(int key) {
    /* GAS, BRAKE/REVERSE, HANDBRAKE, REPAIR, RECOVER, CAMERA, MAP, PAUSE.
     * Follow the user's loaded keymap rather than assume PC default keys. */
    static const int actions[7] = {48, 49, 53, KEYMAP_REPAIR, KEYMAP_RECOVERY, KEYMAP_COCKPIT, KEYMAP_MAP};
    unsigned mask = active_mask | gamepad_mask;
    for (int i = 0; i < 7; ++i)
        if ((mask & (1u << i)) && gKey_mapping[actions[i]] == key) return 1;
    return (active_mask & 128) && key == KEY_ESCAPE;
}
JNIEXPORT void JNICALL
Java_com_dethrace_android_DethraceActivity_nativeTilt(JNIEnv* env, jobject obj, jint axis, jboolean active) {
    SDL_AtomicSet(&tilt_axis, axis < -65535 ? -65535 : axis > 65535 ? 65535 : axis);
    SDL_AtomicSet(&tilt_active, active);
}
void DRAndroidSetTiltWithController(int combine) { tilt_with_controller = combine != 0; }
int DRAndroidSteering(void) {
    /* Priority: a finger on the steering pad, then a controller in use (alone
     * or combined with tilt), then tilt, then an idle controller
     * (100000 = no steering input). */
    if (SDL_AtomicGet(&touch_axis_active)) return SDL_AtomicGet(&touch_axis);
    if (SDL_AtomicGet(&tilt_active)) {
        int tilt = SDL_AtomicGet(&tilt_axis);
        if (SDL_AtomicGet(&using_gamepad) && gamepad_steering != 100000) {
            if (!tilt_with_controller) return gamepad_steering;
            int sum = gamepad_steering + tilt;
            return sum < -65535 ? -65535 : sum > 65535 ? 65535 : sum;
        }
        return tilt;
    }
    return gamepad_steering;
}
static void store_max(SDL_atomic_t* slot, int value) {
    int previous;
    do { previous = SDL_AtomicGet(slot); if (previous >= value) return; }
    while (!SDL_AtomicCAS(slot, previous, value));
}
int DRAndroidImpactStrength(float delta_v) {
    /* Velocity change in m/s during a collision frame. Scrapes (< 1.5 m/s)
     * are ignored; ~16.5 m/s (about 37 mph lost at once) is full strength. */
    if (delta_v < 1.5f) return 0;
    int strength = 150 + (int)((delta_v - 1.5f) * (850.f / 15.f));
    return strength > 1000 ? 1000 : strength;
}
void DRAndroidImpact(int collision, const float velocity[3]) {
    static float last_v[3];
    static Uint32 last_sample, last_pulse;
    static int has_last, last_strength;
    Uint32 now = SDL_GetTicks();
    /* No reference after pauses/menus: a respawn or replay jump is not a hit. */
    int usable = has_last && now - last_sample < 250;
    float dx = velocity[0] - last_v[0], dy = velocity[1] - last_v[1], dz = velocity[2] - last_v[2];
    memcpy(last_v, velocity, sizeof(last_v));
    has_last = 1;
    last_sample = now;
    if (!usable || !collision) return;
    int strength = DRAndroidImpactStrength(sqrtf(dx * dx + dy * dy + dz * dz));
    /* Continuous scraping re-triggers at most every 120 ms unless it gets harder. */
    if (!strength || (now - last_pulse < 120 && strength <= last_strength)) return;
    store_max(&impact, strength);
    store_max(&controller_rumble, strength);
    last_pulse = now;
    last_strength = strength;
}
int DRAndroidTakeControllerRumble(void) {
    return SDL_AtomicSet(&controller_rumble, 0);
}
JNIEXPORT jint JNICALL
Java_com_dethrace_android_DethraceActivity_nativeTakeImpact(JNIEnv* env, jobject obj) {
    return SDL_AtomicSet(&impact, 0);
}

void DRAndroidStartText(const char* text, int max_length) {
    SDL_AtomicLock(&text_lock);
    int session = ++text_session;
    text_pending = 0;
    text_limit = max_length < 1 ? 1 : max_length > 100 ? 100 : max_length;
    SDL_AtomicUnlock(&text_lock);
    JNIEnv* env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID show = (*env)->GetMethodID(env, cls, "showGameTextInput", "(ILjava/lang/String;I)V");
    /* Game data uses a legacy character set; the editor supports printable ASCII. */
    char clean[101]; int n = 0;
    while (text[n] && n < 100) { clean[n] = (unsigned char)text[n] >= 32 && (unsigned char)text[n] < 127 ? text[n] : '?'; ++n; }
    clean[n] = 0;
    jstring initial = (*env)->NewStringUTF(env, clean);
    if (show) (*env)->CallVoidMethod(env, activity, show, session, initial, text_limit);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, initial);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
}
JNIEXPORT void JNICALL
Java_com_dethrace_android_DethraceActivity_nativeSubmitText(JNIEnv* env, jobject obj, jint session, jstring value, jboolean accept) {
    const char* utf = (*env)->GetStringUTFChars(env, value, NULL);
    if (!utf) return;
    SDL_AtomicLock(&text_lock);
    if (session == text_session) {
        int n = 0;
        for (int i = 0; utf[i] && n < text_limit; ++i)
            if ((unsigned char)utf[i] >= 32 && (unsigned char)utf[i] < 127) text_reply[n++] = utf[i];
        text_reply[n] = 0;
        text_pending = accept ? 1 : 2;
    }
    SDL_AtomicUnlock(&text_lock);
    (*env)->ReleaseStringUTFChars(env, value, utf);
}
int DRAndroidPollText(int slot) {
    char reply[101];
    SDL_AtomicLock(&text_lock);
    int result = text_pending;
    text_pending = 0;
    memcpy(reply, text_reply, sizeof(reply));
    SDL_AtomicUnlock(&text_lock);
    if (result == 1 && slot >= 0) {
        gCurrent_position = strlen(gCurrent_typing);
        while (gCurrent_position > 0) DoRLBackspace(slot);
        for (int i = 0; reply[i]; ++i) DoRLTypeLetter(reply[i], slot);
    }
    return result;
}
void DRAndroidCloseText(void) {
    SDL_AtomicLock(&text_lock);
    ++text_session; text_pending = 0;
    SDL_AtomicUnlock(&text_lock);
    JNIEnv* env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    if (!env || !activity) return;
    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID close = (*env)->GetMethodID(env, cls, "closeGameTextInput", "()V");
    if (close) (*env)->CallVoidMethod(env, activity, close);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);
}
#endif
