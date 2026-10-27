#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "harness/hooks.h"
#include "globvars.h"
#include "input.h"
#include "constants.h"
extern tU32 gScan_code[123];
extern void Harness_Platform_Init(tHarness_platform*);
static void event(SDL_Scancode sc,int down){
    SDL_Event e;memset(&e,0,sizeof(e));e.type=down?SDL_KEYDOWN:SDL_KEYUP;
    e.key.keysym.scancode=sc;assert(SDL_PushEvent(&e)==1);
}
static uint8_t state[256];
static void poll(void){MSG_ msg={0};assert(!gHarness_platform.ProcessWindowMessages(&msg));gHarness_platform.GetKeyboardState(sizeof(state),state);}
int main(void){
    Harness_Platform_Init(&gHarness_platform);
    void* window=gHarness_platform.CreateWindowAndRenderer("Input test",0,0,320,200);
    for(int i=0;i<67;i++)gKey_mapping[i]=i;
    for(int i=0;i<123;i++)gScan_code[i]=i;
    event(SDL_SCANCODE_SPACE,1);poll();assert(state[0x1c]); /* A accepts in menus */
    event(SDL_SCANCODE_SPACE,0);gProgram_state.racing=1;
    event(SDL_SCANCODE_SPACE,1);event(SDL_SCANCODE_LEFT,1);poll();assert(state[48]&&state[46]);
    event(SDL_SCANCODE_SPACE,0);event(SDL_SCANCODE_LEFT,0);
    event(SDL_SCANCODE_LCTRL,1);event(SDL_SCANCODE_E,1);poll();assert(state[49]&&state[KEYMAP_REPAIR]);
    event(SDL_SCANCODE_LCTRL,0);event(SDL_SCANCODE_E,0);
    event(SDL_SCANCODE_LSHIFT,1);event(SDL_SCANCODE_LALT,1);poll();assert(state[53]&&state[KEYMAP_RECOVERY]);
    SDL_Event e;memset(&e,0,sizeof(e));e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;assert(SDL_PushEvent(&e)==1);
    poll();for(int i=0;i<256;i++)assert(!state[i]);
    event(SDL_SCANCODE_ESCAPE,1);event(SDL_SCANCODE_RETURN,1);MSG_ msg={0};assert(gHarness_platform.ProcessWindowMessages(&msg));assert(msg.message==WM_QUIT);
    gHarness_platform.DestroyWindow(window);
    puts("PASS input: menus, simultaneous steering/accelerator, brake/repair, handbrake/recovery, focus release, exit chord");
}
