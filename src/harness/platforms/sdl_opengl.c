#include "harness/profiler.h"
#ifdef __ANDROID__
#include "frame_pacer.h"
#include <time.h>
static DRFramePacer frame_pacer;
#endif
#include <glad/glad.h>

// this needs to be included after glad.h
#include <SDL.h>
#include <SDL_opengl.h>

#include "../renderers/gl/gl_renderer.h"
#include "harness/config.h"
#include "harness/hooks.h"
#include "harness/trace.h"
#include "sdl2_scancode_to_dinput.h"
#include "screen_layout.h"

#include "globvars.h"
#include "grafdata.h"
#include "pd/sys.h"

#ifdef __ANDROID__
#include "android_widescreen.h"
#include "android_controller.h"
static void dr_close_controllers(void);
#include "../../DETHRACE/android_input.h"
#endif

SDL_Window* window;
SDL_GLContext context;
uint8_t directinput_key_state[SDL_NUM_SCANCODES];
int render_width, render_height;
int window_width, window_height;
int vp_x, vp_y, vp_width, vp_height;

static DRScreenLayout screen_layout;

static void update_viewport(void) {
    int drawable_width, drawable_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
    const int stretch = 0; /* Always preserve the chosen canvas aspect. */
    if (!DRScreenLayoutInit(&screen_layout, window_width, window_height,
        drawable_width, drawable_height, render_width, render_height, stretch)) return;
    vp_x = screen_layout.x; vp_y = screen_layout.y;
    vp_width = screen_layout.w; vp_height = screen_layout.h;
    GLRenderer_SetViewport(vp_x, vp_y, vp_width, vp_height);
    LOG_INFO("display: window %dx%d drawable %dx%d game %dx%d viewport %d,%d %dx%d fill=%d",
        window_width, window_height, drawable_width, drawable_height,
        render_width, render_height, vp_x, vp_y, vp_width, vp_height, stretch);
}

static void create_glcore_context(char* title) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    context = SDL_GL_CreateContext(window);
}

static void create_gles_context(char* title) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    context = SDL_GL_CreateContext(window);
}

static void* create_window_and_renderer(char* title, int x, int y, int width, int height) {
#ifdef __ANDROID__
    DRAndroid_ConfigureAspect(&width, &height);
    dr_bindings_loaded = 0;
#endif

    window_width = width;
    window_height = height;
    render_width = width;
    render_height = height;
    tOpenGL_profile opengl_profile;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        LOG_PANIC("SDL_INIT_VIDEO error: %s", SDL_GetError());
    }

#ifdef __ANDROID__
    {
        extern int dr_app_event_watch(void* userdata, SDL_Event* event);
        SDL_AddEventWatch(dr_app_event_watch, NULL);
    }
#endif

#ifdef __ANDROID__
    // Android only has GLES; skip desktop core profile attempt
    create_gles_context(title);
    opengl_profile = eOpenGL_profile_es;
#else
    // prefer OpenGL core profile
    create_glcore_context(title);
    opengl_profile = eOpenGL_profile_core;
    if (window == NULL || context == NULL) {
        if (window != NULL) {
            SDL_DestroyWindow(window);
        }

        LOG_WARN("Failed to create OpenGL core profile: %s. Trying OpenGLES...", SDL_GetError());
        // fallback to OpenGL ES 3
        create_gles_context(title);
        opengl_profile = eOpenGL_profile_es;
    }
#endif

    if (window == NULL || context == NULL) {
        LOG_PANIC("Failed to create OpenGL context: %s", SDL_GetError());
    }


    if (harness_game_config.start_full_screen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    // Load GL extensions using glad
#ifdef __ANDROID__
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
        LOG_PANIC("Failed to initialize the OpenGL ES context with GLAD.");
        exit(1);
    }
#else
    if (opengl_profile == eOpenGL_profile_es) {
        if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress)) {
            LOG_PANIC("Failed to initialize the OpenGL ES context with GLAD.");
            exit(1);
        }
    } else if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        LOG_PANIC("Failed to initialize the OpenGL context with GLAD.");
        exit(1);
    }
#endif

#ifdef __ANDROID__
    /* The shared presenter supplies the selected software cap. Avoid a
     * second vsync wait; Android may still enforce display pacing. */
    SDL_GL_SetSwapInterval(0);
#else
    SDL_GL_SetSwapInterval(1);
#endif

#ifdef __ANDROID__
    /* Render the 3D scene at 2× (standard) or 3× (high) pixmap resolution and downsample / present
     * via the super-res-aware framebuffer shader. The 2D HUD/menu drawing
     * stays at pixmap resolution. */
    const char* resolution = getenv("DETHRACE_RENDER_QUALITY");
    GLRenderer_SetSuperFactor(resolution && strcmp(resolution,"performance")==0 ? 1 :
                              resolution && strcmp(resolution,"high")==0 ? 3 : 2);
#endif
    GLRenderer_Init(opengl_profile, render_width, render_height);
    update_viewport();

    return window;
}

static int set_window_pos(void* hWnd, int x, int y, int nWidth, int nHeight) {
    // SDL_SetWindowPosition(hWnd, x, y);
    if (nWidth == 320 && nHeight == 200) {
        nWidth = 640;
        nHeight = 400;
    }
#ifdef __ANDROID__
    /* The engine's SetWindowPos calls describe game pixels, not Android's
     * SurfaceView size. Resizing the physical surface here causes cropping. */
    (void)hWnd;
    (void)nWidth;
    (void)nHeight;
#else
    SDL_SetWindowSize(hWnd, nWidth, nHeight);
#endif
    return 0;
}

static void destroy_window(void* hWnd) {
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
#ifdef __ANDROID__
    dr_close_controllers();
#endif
    SDL_Quit();
    window = NULL;
}

// Checks whether the `flag_check` is the only modifier applied.
// e.g. is_only_modifier(event.key.keysym.mod, KMOD_ALT) returns true when only the ALT key was pressed
static int is_only_key_modifier(int modifier_flags, int flag_check) {
    return (modifier_flags & flag_check) && (modifier_flags & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI)) == (modifier_flags & flag_check);
}

#ifdef __ANDROID__
extern void Audio_SetPaused(int paused);

int dr_app_event_watch(void* userdata, SDL_Event* event) {
    (void)userdata;
    switch (event->type) {
    case SDL_APP_WILLENTERBACKGROUND:
    case SDL_APP_DIDENTERBACKGROUND:
        Audio_SetPaused(1);
        break;
    case SDL_APP_WILLENTERFOREGROUND:
    case SDL_APP_DIDENTERFOREGROUND:
        Audio_SetPaused(0);
        break;
    }
    return 1;
}

static SDL_GameController* dr_controllers[4];
static uint8_t dr_controller_keys[SDL_NUM_SCANCODES];
static Sint16 dr_activity_axes[4][SDL_CONTROLLER_AXIS_MAX];
#ifdef __ANDROID__
static void dr_controller_axis_activity(const SDL_ControllerAxisEvent* event) {
    if (event->axis>=SDL_CONTROLLER_AXIS_MAX) return;
    for (int i=0;i<4;i++) {
        if (!dr_controllers[i] || SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(dr_controllers[i]))!=event->which) continue;
        int previous=dr_activity_axes[i][event->axis];
        int value=event->value;
        int trigger=event->axis==SDL_CONTROLLER_AXIS_TRIGGERLEFT || event->axis==SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
        int active=trigger ? value>4000 : abs(value)>8000;
        if (!active) dr_activity_axes[i][event->axis]=0;
        else if (abs(value-previous)>4000) {
            dr_activity_axes[i][event->axis]=value;
            DRAndroidGamepadActivity(event->timestamp);
        }
    }
}
#endif

static void dr_controller_key(SDL_Scancode sc, int down) {
    int key = sdlScanCodeToDirectInputKeyNum[sc];
    if (key && down) dr_controller_keys[key] = 0x80;
}

static void dr_close_controllers(void) {
    for (int i = 0; i < 4; ++i) {
        if (dr_controllers[i]) SDL_GameControllerClose(dr_controllers[i]);
        dr_controllers[i] = NULL;
        dr_input_latches[i] = 0;
        memset(dr_activity_axes[i], 0, sizeof(dr_activity_axes[i]));
    }
    DRAndroidSetGamepadButtons(0);
    DRAndroidSetGamepadSteering(100000);
}
static void dr_poll_controllers(void) {
    dr_load_bindings();
    memset(dr_controller_keys, 0, sizeof(dr_controller_keys));
    /* Reconcile by instance ID so reconnects do not retain stale input. */
    for (int i = 0; i < 4; ++i) {
        if (dr_controllers[i] && !SDL_GameControllerGetAttached(dr_controllers[i])) {
            SDL_GameControllerClose(dr_controllers[i]);
            dr_controllers[i] = NULL;
            dr_input_latches[i] = 0;
            memset(dr_activity_axes[i],0,sizeof(dr_activity_axes[i]));
        }
    }
    for (int device = 0; device < SDL_NumJoysticks(); ++device) {
        if (!SDL_IsGameController(device)) continue;
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device);
        int found = 0, empty = -1;
        for (int i = 0; i < 4; ++i) {
            if (!dr_controllers[i]) { if (empty < 0) empty = i; continue; }
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(dr_controllers[i])) == id) found = 1;
        }
        if (!found && empty >= 0) dr_controllers[empty] = SDL_GameControllerOpen(device);
    }
    SDL_GameControllerUpdate();
    for (int i = 0; i < 4; ++i)
        if (!dr_controllers[i] || !SDL_GameControllerGetAttached(dr_controllers[i])
            || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
            || !gProgram_state.racing || gInterface_within_race_mode) dr_input_latches[i] = 0;
    int steering = 100000;
    unsigned buttons = 0;
    for (int i = 0; i < 4; ++i) {
        SDL_GameController* c = dr_controllers[i];
        if (!c || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) continue;
        int x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
        int y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
        int left = x < -8000 || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        int right = x > 8000 || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        int up = y < -8000 || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP);
        int down = y > 8000 || SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        if (gProgram_state.racing && !gInterface_within_race_mode) {
            int axis = 0;
            if (dr_controller_steering_mode != 3) {
                int stick = dr_controller_steering_mode == 2
                    ? SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX) : x;
                axis = DRAndroidNormalizeStick(stick);
            }
            if (dr_controller_steering_mode == 0 || dr_controller_steering_mode == 3) {
                int dpad = SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                         - SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                if (dpad) axis = dpad * 65535;
            }
            if (steering == 100000 || abs(axis) > abs(steering)) steering = axis;
            for (int action = 0; action < 7; ++action)
                if (dr_bound_input(c, action, i)) buttons |= 1u << action;
            if (dr_bound_input(c, 10, i)) buttons |= 1u;
            if (dr_bound_input(c, 8, i)) buttons |= 1u << 8;
            if (dr_bound_input(c, 9, i)) buttons |= 1u << 9;
        } else {
            dr_controller_key(SDL_SCANCODE_LEFT, left);
            dr_controller_key(SDL_SCANCODE_RIGHT, right);
            dr_controller_key(SDL_SCANCODE_UP, up);
            dr_controller_key(SDL_SCANCODE_DOWN, down);
            dr_controller_key(SDL_SCANCODE_RETURN, dr_bound_input(c, 11, i));
            dr_controller_key(SDL_SCANCODE_ESCAPE, dr_bound_input(c, 12, i));
        }
        dr_controller_key(SDL_SCANCODE_ESCAPE, dr_bound_input(c, 7, i));
    }
    DRAndroidSetGamepadSteering(steering);
    // Polling rebuilds the mask, clearing released/disconnected/background input.
    DRAndroidSetGamepadButtons(buttons);
}

#endif /* __ANDROID__ */

static int get_and_handle_message(MSG_* msg) {
    SDL_Event event;
    int dinput_key;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_RETURN) {
                if (event.key.type == SDL_KEYDOWN) {
                    if ((event.key.keysym.mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_GUI))) {
                        // Ignore keydown of RETURN when used together with some modifier
                        return 0;
                    }
                } else if (event.key.type == SDL_KEYUP) {
                    if (is_only_key_modifier(event.key.keysym.mod, KMOD_ALT)) {
                        SDL_SetWindowFullscreen(window, (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                }
            }

            // Map incoming SDL scancode to DirectInput DIK_* key code.
            // https://github.com/DanielGibson/Snippets/blob/master/sdl2_scancode_to_dinput.h
            dinput_key = sdlScanCodeToDirectInputKeyNum[event.key.keysym.scancode];
            if (dinput_key == 0) {
                LOG_WARN("unexpected scan code %s (%d)", SDL_GetScancodeName(event.key.keysym.scancode), event.key.keysym.scancode);
                return 0;
            }
            // DInput expects high bit to be set if key is down
            // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418261(v=vs.85)
            directinput_key_state[dinput_key] = (event.type == SDL_KEYDOWN ? 0x80 : 0);
            break;

        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_LOST:
                memset(directinput_key_state, 0, sizeof(directinput_key_state));
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                SDL_GetWindowSize(window, &window_width, &window_height);
                update_viewport();
                break;
            }
            break;


#ifdef __ANDROID__
        case SDL_CONTROLLERBUTTONDOWN:
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
                DRAndroidGamepadActivity(event.cbutton.timestamp);
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
                dr_controller_axis_activity(&event.caxis);
            break;
#endif
        case SDL_QUIT:
            msg->message = WM_QUIT;
            return 1;
        }
    }
    return 0;
}

static void swap_window(void) {
#ifdef __ANDROID__
    uint64_t limiter_start=DRProfileClock();
    uint64_t frequency=SDL_GetPerformanceFrequency(), remaining;
    unsigned cap=harness_game_config.fps>0?(unsigned)harness_game_config.fps:0;
    while ((remaining=DRFrameWait(&frame_pacer,SDL_GetPerformanceCounter(),frequency,cap))!=0) {
        uint64_t ns=(remaining*1000000000ull+frequency-1)/frequency;
        struct timespec delay={(time_t)(ns/1000000000ull),(long)(ns%1000000000ull)};
        nanosleep(&delay,NULL);
    }
    DRFrameCommit(&frame_pacer,SDL_GetPerformanceCounter());
    DRProfileTime(DRP_LIMITER,limiter_start);
#endif
    uint64_t profile_swap = DRProfileClock();
    SDL_GL_SwapWindow(window);
    DRProfileTime(DRP_SWAP, profile_swap);
#ifdef __ANDROID__
    DRAndroidFramePresented();
#endif
}

static void get_keyboard_state(unsigned int count, uint8_t* buffer) {
    if (count > sizeof(directinput_key_state)) count = sizeof(directinput_key_state);
#ifdef __ANDROID__
    dr_poll_controllers();
    for (unsigned int i = 0; i < count; ++i)
        buffer[i] = directinput_key_state[i] | dr_controller_keys[i];
#else
    memcpy(buffer, directinput_key_state, count);
#endif
}

static int map_mouse_position(int* x, int* y) {
    int wx, wy;
    SDL_GetMouseState(&wx, &wy);
    int doubled_menu = 0;
#if defined(DETHRACE_FIX_BUGS)
    doubled_menu = gGraf_data_index == 0 && gReal_graf_data_index != 0;
#endif
    return DRScreenMapPointer(&screen_layout, wx, wy, doubled_menu, x, y);
}

static int get_mouse_buttons(int* pButton1, int* pButton2) {
    int state = SDL_GetMouseState(NULL, NULL);
    int x, y;
    /* A tap in a letterbox/menu margin must not click a clamped edge item. */
    if (!map_mouse_position(&x, &y)) state = 0;
    *pButton1 = state & SDL_BUTTON_LMASK;
    *pButton2 = state & SDL_BUTTON_RMASK;
    return 0;
}

static int get_mouse_position(int* pX, int* pY) {
    map_mouse_position(pX, pY);
    return 0;
}

static void set_palette(PALETTEENTRY_* pal) {
    GLRenderer_SetPalette((uint8_t*)pal);
}

int show_error_message(void* window, char* text, char* caption) {
    fprintf(stderr, "%s", text);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, caption, text, window);
    return 0;
}

void Harness_Platform_Init(tHarness_platform* platform) {
    platform->ProcessWindowMessages = get_and_handle_message;
    platform->Sleep = SDL_Delay;
    platform->GetTicks = SDL_GetTicks;
    platform->CreateWindowAndRenderer = create_window_and_renderer;
    platform->ShowCursor = SDL_ShowCursor;
    platform->SetWindowPos = set_window_pos;
    platform->SwapWindow = swap_window;
    platform->DestroyWindow = destroy_window;
    platform->GetKeyboardState = get_keyboard_state;
    platform->GetMousePosition = get_mouse_position;
    platform->GetMouseButtons = get_mouse_buttons;
    platform->DestroyWindow = destroy_window;
    platform->ShowErrorMessage = show_error_message;

    platform->Renderer_BufferModel = GLRenderer_BufferModel;
    platform->Renderer_BufferMaterial = GLRenderer_BufferMaterial;
    platform->Renderer_BufferTexture = GLRenderer_BufferTexture;
    platform->Renderer_SetPalette = set_palette;
    platform->Renderer_FullScreenQuad = GLRenderer_FullScreenQuad;
    platform->Renderer_Model = GLRenderer_Model;
    platform->Renderer_ClearBuffers = GLRenderer_ClearBuffers;
    platform->Renderer_FlushBuffers = GLRenderer_FlushBuffers;
    platform->Renderer_BeginScene = GLRenderer_BeginScene;
    platform->Renderer_EndScene = GLRenderer_EndScene;
    platform->Renderer_SetViewport = GLRenderer_SetViewport;
}
