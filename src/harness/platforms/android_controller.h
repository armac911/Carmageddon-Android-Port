/* Android launcher binding IDs: SDL buttons 0..20; axis directions 100..111. */
#ifndef DR_ANDROID_CONTROLLER_H
#define DR_ANDROID_CONTROLLER_H
#include <stdlib.h>
#include "../../DETHRACE/android_control_tuning.h"
static int dr_bindings[13] = {111,109,9,10,3,8,4,6,106,107,2,0,1};
static int dr_bindings_loaded;
static int dr_controller_steering_mode;
static unsigned dr_input_latches[4];
static void dr_load_bindings(void) {
    if (dr_bindings_loaded) return;
    dr_bindings_loaded = 1;
    dr_control_tuning(1);
    static const int defaults[13] = {111,109,9,10,3,8,4,6,106,107,2,0,1};
    for (int i=0;i<13;++i) dr_bindings[i]=defaults[i];
    dr_controller_steering_mode=0;
    const char* value = getenv("DETHRACE_CONTROLLER_BINDINGS");
    for (int i = 0; value && *value && i < 13; ++i) {
        char* end;
        long id = strtol(value, &end, 10);
        if (end == value || (*end && *end != ',')) break;
        if (id == -1 || (id >= 0 && id < SDL_CONTROLLER_BUTTON_MAX)
            || (id >= 100 && id <= 111)) dr_bindings[i] = (int)id;
        value = *end ? end + 1 : end;
    }
    const char* mode = getenv("DETHRACE_CONTROLLER_STEERING");
    if (mode) dr_controller_steering_mode = atoi(mode);
    if (dr_controller_steering_mode < 0 || dr_controller_steering_mode > 3) dr_controller_steering_mode = 0;
}
static int dr_bound_input(SDL_GameController* c, int slot, int controller) {
    int id = dr_bindings[slot];
    if (id < 0) return 0;
    if (id < SDL_CONTROLLER_BUTTON_MAX) return SDL_GameControllerGetButton(c, (SDL_GameControllerButton)id);
    int axis = (id - 100) / 2;
    int value = SDL_GameControllerGetAxis(c, (SDL_GameControllerAxis)axis);
    int magnitude = ((id - 100) & 1) ? value : -value;
    unsigned bit = 1u << slot;
    DRControlTuning* tuning = dr_control_tuning(0);
    int threshold = 32767 * (axis >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? tuning->trigger_deadzone : tuning->action_deadzone) / 100;
    if (magnitude <= threshold / 2) dr_input_latches[controller] &= ~bit;
    if (magnitude > threshold) dr_input_latches[controller] |= bit;
    return (dr_input_latches[controller] & bit) != 0;
}
static int dr_bound_pedal(SDL_GameController* c, int slot) {
    int id = dr_bindings[slot];
    if (id < 0) return 0;
    if (id < SDL_CONTROLLER_BUTTON_MAX)
        return SDL_GameControllerGetButton(c, (SDL_GameControllerButton)id) ? 65535 : 0;
    int axis = (id - 100) / 2;
    int raw = SDL_GameControllerGetAxis(c, (SDL_GameControllerAxis)axis);
    int magnitude = ((id - 100) & 1) ? raw : -raw;
    DRControlTuning* cfig = dr_control_tuning(0);
    int dz = axis >= SDL_CONTROLLER_AXIS_TRIGGERLEFT ? cfig->trigger_deadzone : cfig->action_deadzone;
    return magnitude > 0 ? dr_control_axis(magnitude, 32767, dz, cfig->trigger_sensitivity) : 0;
}
#endif
