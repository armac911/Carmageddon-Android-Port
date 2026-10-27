/* Exercise real platform selection with software listed BEFORE Mini, while
 * using a desktop dummy renderer behind the injected device calls. */
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "harness/hooks.h"
extern void Harness_Platform_Init(tHarness_platform*);
static int selected;
const char* __wrap_SDL_GetCurrentVideoDriver(void){return "Mini";}
int __wrap_SDL_GetNumRenderDrivers(void){return 2;}
int __wrap_SDL_GetRenderDriverInfo(int i,SDL_RendererInfo* info){
    memset(info,0,sizeof(*info));
    info->name=i==0?"software":"Miyoo Mini";
    info->flags=i==0?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED;
    return 0;
}
extern SDL_Renderer* __real_SDL_CreateRenderer(SDL_Window*,int,Uint32);
SDL_Renderer* __wrap_SDL_CreateRenderer(SDL_Window* w,int i,Uint32 flags){
    /* The old index=-1, flags=0 path fails this regression assertion. */
    assert(i==1);assert(flags&SDL_RENDERER_ACCELERATED);selected++;
    return __real_SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
}
int main(void){
    Harness_Platform_Init(&gHarness_platform);
    void* window=gHarness_platform.CreateWindowAndRenderer("Presenter selection test",0,0,320,200);
    assert(window);assert(selected==1);
    assert(!strcmp(SDL_GetHint(SDL_HINT_RENDER_BATCHING),"0"));
    gHarness_platform.DestroyWindow(window);
    puts("PASS Mini selects explicit 2D presenter even when software comes first and environment requests software");
    return 0;
}
