#ifdef __ANDROID__
#include <jni.h>
#include <SDL.h>
#include <SDL_system.h>
#include <string.h>
#include <stdlib.h>
#include "android_input.h"
#include "android_control_tuning.h"
#include "harness/profiler.h"
JNIEXPORT void JNICALL Java_com_dethrace_android_DethraceActivity_nativeProfilePause(JNIEnv* env,jobject obj) { DRProfileMarkPause(); }
#include "common/globvars.h"
#include "common/input.h"
#include "constants.h"

static SDL_atomic_t touch_mask, touch_edges, touch_axis, touch_axis_active, touch_reset, impact;
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
    const char* mode = getenv("DETHRACE_STEERING");
    return mode && strcmp(mode, "digital") == 0;
}
int DRAndroidNormalizeStick(int value) {
    DRControlTuning* c = dr_control_tuning(0);
    return dr_control_axis(value, 32767, c->stick_deadzone, c->stick_sensitivity);
}
static int gamepad_digital, gamepad_acc = -1, gamepad_dec = -1;
static float filtered_steer, filtered_acc, filtered_dec;
static int touch_steer_return;
void DRAndroidInitControlTuning(void) {
    dr_control_tuning(1);
    filtered_steer = filtered_acc = filtered_dec = 0;
    touch_steer_return = 0;
    gamepad_acc = gamepad_dec = -1;
    gamepad_digital = 0;
    gamepad_steering = 100000;
}
void DRAndroidSetGamepadDigital(int digital) { gamepad_digital = digital; }
void DRAndroidSetGamepadPedals(int acc, int dec) { gamepad_acc = acc; gamepad_dec = dec; }
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
    if (SDL_AtomicSet(&touch_reset, 0)) {
        memset(held_until, 0, sizeof(held_until));
        filtered_steer = filtered_acc = filtered_dec = 0;
        touch_steer_return = 0;
    }
    unsigned edges = SDL_AtomicSet(&touch_edges, 0);
    unsigned down = SDL_AtomicGet(&touch_mask);
    Uint32 now = SDL_GetTicks();
    active_mask = 0;
    if (!gProgram_state.racing || gInterface_within_race_mode) {
        filtered_steer = filtered_acc = filtered_dec = 0;
        touch_steer_return = 0;
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
int DRAndroidSteering(void) {
    return SDL_AtomicGet(&touch_axis_active) ? SDL_AtomicGet(&touch_axis) : gamepad_steering;
}
int DRAndroidFilterSteering(int target, unsigned dt) {
    DRControlTuning* c = dr_control_tuning(0);
    int touch = SDL_AtomicGet(&touch_axis_active);
    if (!gProgram_state.racing || gInterface_within_race_mode) {
        filtered_steer = 0;
        touch_steer_return = 0;
        return 100000;
    }
    if (touch) touch_steer_return = 1;
    else if (target != 100000) touch_steer_return = 0;
    if (target == 100000 && touch_steer_return) target = 0;
    if (target == 100000) { filtered_steer = 0; return target; }
    if (touch) target = dr_control_axis(target, 65535, c->touch_deadzone, c->touch_sensitivity);
    if ((touch || !gamepad_digital) && DRAndroidDigitalSteering())
        target = target < 0 ? -65535 : target > 0 ? 65535 : 0;
    if ((gamepad_digital && !touch) || DRAndroidDigitalSteering())
        target = target * c->dpad_sensitivity / 100;
    filtered_steer = dr_control_slew(filtered_steer, target, dt, c->steer_rise, c->steer_return);
    if (!touch && target == 0 && filtered_steer == 0.f) touch_steer_return = 0;
    return (int)filtered_steer;
}
int DRAndroidPedals(int* acc, int* dec, unsigned dt) {
    unsigned mask = (unsigned)SDL_AtomicGet(&touch_mask) | gamepad_mask;
    if (!gProgram_state.racing || gInterface_within_race_mode ||
        (gamepad_acc < 0 && !((mask | active_mask) & 3u))) {
        filtered_acc = filtered_dec = 0;
        return 0;
    }
    int a = mask & 1u ? 65535 : gamepad_acc > 0 ? gamepad_acc : 0;
    int b = mask & 2u ? 65535 : gamepad_dec > 0 ? gamepad_dec : 0;
    DRControlTuning* c = dr_control_tuning(0);
    filtered_acc = dr_control_slew(filtered_acc, a, dt, c->pedal_rise, 0);
    filtered_dec = dr_control_slew(filtered_dec, b, dt, c->pedal_rise, 0);
    *acc = filtered_acc > 0 ? (int)filtered_acc : -1;
    *dec = filtered_dec > 0 ? (int)filtered_dec : -1;
    return 1;
}
void DRAndroidImpact(int collision, float speed) {
    static int was_collision;
    static Uint32 last;
    Uint32 now = SDL_GetTicks();
    if (collision && !was_collision && speed > 5.f && now - last > 500) {
        SDL_AtomicSet(&impact, 1);
        last = now;
    }
    was_collision = collision;
}
JNIEXPORT jboolean JNICALL
Java_com_dethrace_android_DethraceActivity_nativeTakeImpact(JNIEnv* env, jobject obj) {
    return SDL_AtomicSet(&impact, 0) != 0;
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
