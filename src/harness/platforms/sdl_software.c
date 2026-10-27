/* SDL2 presenter and physical keyboard input for Miyoo Mini Plus. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include "harness/hooks.h"
#include "harness/trace.h"
#include "../renderers/software/software.h"
#include "sdl2_scancode_to_dinput.h"
#include "globvars.h"
#include "input.h"
#include "constants.h"

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* texture;
static uint32_t* pixels;
static uint32_t palette[256];
static int width,height;
static uint8_t held[SDL_NUM_SCANCODES];
static unsigned frames,report_time;
static int quit_requested;
/* Original PC key-map action slots used by PollCarControls. */
enum { STEER_LEFT=46,STEER_RIGHT=47,ACCELERATE=48,BRAKE=49,HANDBRAKE=53,GEAR_DOWN=55,HORN=58 };
extern tU32 gScan_code[123];

static void fail(const char* where){fprintf(stderr,"%s: %s\n",where,SDL_GetError());exit(1);}
static void destroy(void* unused){
    (void)unused;
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
    free(pixels);pixels=NULL;SDL_Quit();
}
static void* create(char* title,int x,int y,int w,int h){
    (void)x;(void)y;
    if(w<=0||h<=0||w>640||h>480){fprintf(stderr,"Unsupported canvas %dx%d\n",w,h);exit(1);}
    width=w;height=h;
    setvbuf(stdout,NULL,_IONBF,0);setvbuf(stderr,NULL,_IONBF,0);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER))fail("SDL_Init");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    window=SDL_CreateWindow(title,0,0,640,480,SDL_WINDOW_SHOWN);
    if(!window)fail("SDL_CreateWindow");
    /* Mini's generic software framebuffer update is a no-op in this SDL fork.
     * Its software renderer precedes Mini in SDL's default driver list, so flags
     * 0 silently selects an invisible surface. Select the MI_GFX 2D presenter
     * explicitly; CPU 3D rasterization is independent of this SDL flag. */
    const char* video=SDL_GetCurrentVideoDriver();
    int is_mini=video && (!SDL_strcasecmp(video,"Mini") || !SDL_strcasecmp(video,"mmiyoo"));
    int mini_index=-1;
    for(int i=0;i<SDL_GetNumRenderDrivers();i++) {
        SDL_RendererInfo available;
        if(SDL_GetRenderDriverInfo(i,&available))continue;
        printf("available renderer index=%d name=%s flags=0x%x\n",i,available.name,(unsigned)available.flags);
        if(!SDL_strcasecmp(available.name,"Miyoo Mini") || !SDL_strcasecmp(available.name,"mmiyoo"))mini_index=i;
    }
    SDL_SetHintWithPriority(SDL_HINT_RENDER_BATCHING,"0",SDL_HINT_OVERRIDE);
    if(is_mini) {
        if(mini_index<0){fprintf(stderr,"Mini video driver has no MI_GFX presenter; refusing invisible software fallback\n");exit(1);}
        renderer=SDL_CreateRenderer(window,mini_index,SDL_RENDERER_ACCELERATED);
    } else {
        /* Desktop/dummy backends used by procedural tests support software. */
        renderer=SDL_CreateRenderer(window,-1,0);
        if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    }
    if(!renderer)fail("SDL_CreateRenderer");
    texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
    if(!texture)fail("SDL_CreateTexture");
    pixels=calloc((size_t)w*h,sizeof(*pixels));
    if(!pixels){fprintf(stderr,"Presenter allocation failed\n");exit(1);}
    SDL_RendererInfo info;SDL_GetRendererInfo(renderer,&info);
    printf("MIYOO experimental CPU 3D; SDL=%s renderer=%s canvas=%dx%d output=640x480 4:3\n",
        SDL_GetCurrentVideoDriver(),info.name,w,h);
    SDL_ShowCursor(SDL_DISABLE);report_time=SDL_GetTicks();return window;
}
static int events(MSG_* msg){
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        if(ev.type==SDL_KEYDOWN||ev.type==SDL_KEYUP){
            unsigned sc=ev.key.keysym.scancode;
            if(sc<SDL_NUM_SCANCODES)held[sc]=ev.type==SDL_KEYDOWN;
            if(!ev.key.repeat)printf("input scancode=%u name=%s down=%d\n",sc,SDL_GetScancodeName(sc),ev.type==SDL_KEYDOWN);
            /* Explicit exit chord, no immediate exit on Menu alone. */
            if(held[SDL_SCANCODE_ESCAPE]&&held[SDL_SCANCODE_RETURN])quit_requested=1;
        }
        if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_FOCUS_LOST)memset(held,0,sizeof(held));
        if(ev.type==SDL_QUIT)quit_requested=1;
    }
    if(quit_requested){msg->message=WM_QUIT;return 1;}
    return 0;
}
static void key(uint8_t* dst,unsigned n,SDL_Scancode sc){
    unsigned dik=sdlScanCodeToDirectInputKeyNum[sc];if(dik<n)dst[dik]=0x80;
}
static void action(uint8_t* dst,unsigned n,int mapping){
    int code=gKey_mapping[mapping];
    if(code>=0&&code<123&&gScan_code[code]>0&&gScan_code[code]<n)dst[gScan_code[code]]|=0x80;
}
static void keyboard(unsigned n,uint8_t* dst){
    memset(dst,0,n);
    for(int sc=0;sc<SDL_NUM_SCANCODES;sc++)if(held[sc]){
        /* Physical A/B/X/Y/shoulders are translated below, not PC modifiers. */
        if(sc==SDL_SCANCODE_LCTRL||sc==SDL_SCANCODE_LALT||sc==SDL_SCANCODE_SPACE||sc==SDL_SCANCODE_LSHIFT||sc==SDL_SCANCODE_E||sc==SDL_SCANCODE_T||sc==SDL_SCANCODE_TAB||sc==SDL_SCANCODE_BACKSPACE||sc==SDL_SCANCODE_RCTRL)continue;
        key(dst,n,(SDL_Scancode)sc);
    }
    int racing=gProgram_state.racing;
    if(racing) {
        if(held[SDL_SCANCODE_LEFT])action(dst,n,STEER_LEFT);
        if(held[SDL_SCANCODE_RIGHT])action(dst,n,STEER_RIGHT);
        if(held[SDL_SCANCODE_UP])action(dst,n,ACCELERATE);
        if(held[SDL_SCANCODE_DOWN])action(dst,n,BRAKE);
    }
    if(held[SDL_SCANCODE_SPACE]){if(racing)action(dst,n,ACCELERATE);else key(dst,n,SDL_SCANCODE_RETURN);}
    if(held[SDL_SCANCODE_LCTRL]){if(racing)action(dst,n,BRAKE);else key(dst,n,SDL_SCANCODE_ESCAPE);}
    if(held[SDL_SCANCODE_LSHIFT])action(dst,n,HANDBRAKE);
    if(held[SDL_SCANCODE_LALT])action(dst,n,KEYMAP_RECOVERY);
    if(held[SDL_SCANCODE_E])action(dst,n,KEYMAP_REPAIR);
    if(held[SDL_SCANCODE_T])action(dst,n,KEYMAP_COCKPIT);
    if(held[SDL_SCANCODE_TAB])action(dst,n,GEAR_DOWN);
    if(held[SDL_SCANCODE_BACKSPACE])action(dst,n,HORN);
    if(held[SDL_SCANCODE_RCTRL])action(dst,n,KEYMAP_MAP);
}
static void set_palette(PALETTEENTRY_* p){for(int i=0;i<256;i++)palette[i]=0xff000000u|((uint32_t)p[i].peRed<<16)|((uint32_t)p[i].peGreen<<8)|p[i].peBlue;}
static void present_indexed(uint8_t* src){
    /* Full-screen hook receives tightly packed main canvas pixels. */
    for(int i=0;i<width*height;i++)pixels[i]=palette[src[i]];
    if(SDL_UpdateTexture(texture,NULL,pixels,width*4))fail("SDL_UpdateTexture");
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    int ow,oh;SDL_GetRendererOutputSize(renderer,&ow,&oh);
    SDL_Rect dst={0,0,ow,ow*3/4};
    if(dst.h>oh){dst.h=oh;dst.w=oh*4/3;}dst.x=(ow-dst.w)/2;dst.y=(oh-dst.h)/2;
    if(SDL_RenderCopy(renderer,texture,NULL,&dst))fail("SDL_RenderCopy");
}
static void swap(void){
    SDL_RenderPresent(renderer);frames++;
    unsigned now=SDL_GetTicks();if(now-report_time>=5000){
        struct rusage usage;getrusage(RUSAGE_SELF,&usage);
        printf("metrics fps=%.2f peak_rss_kib=%ld triangles_total=%lu ticks=%u\n",frames*1000.0/(now-report_time),usage.ru_maxrss,SWTriangleCount(),now);
        report_time=now;frames=0;
    }
}
static int mouse_buttons(int* a,int* b){*a=*b=0;return 0;}
static int mouse_position(int* x,int* y){*x=*y=0;return 0;}
static int position(void* a,int x,int y,int w,int h){(void)a;(void)x;(void)y;(void)w;(void)h;return 0;}
static int error(void* a,char* text,char* caption){(void)a;fprintf(stderr,"%s: %s\n",caption,text);return 0;}
void Harness_Platform_Init(tHarness_platform* p){
    memset(p,0,sizeof(*p));
    p->CreateWindowAndRenderer=create;p->DestroyWindow=destroy;p->ProcessWindowMessages=events;
    p->GetKeyboardState=keyboard;p->GetMouseButtons=mouse_buttons;p->GetMousePosition=mouse_position;
    p->SetWindowPos=position;p->ShowCursor=SDL_ShowCursor;p->Sleep=SDL_Delay;p->GetTicks=SDL_GetTicks;
    p->SwapWindow=swap;p->ShowErrorMessage=error;
    p->Renderer_SetPalette=set_palette;p->Renderer_FullScreenQuad=present_indexed;
    p->Renderer_BeginScene=SWBegin;p->Renderer_EndScene=SWEnd;p->Renderer_Model=SWModel;
    p->Renderer_ClearBuffers=SWClear;p->Renderer_BufferTexture=SWTexture;p->Renderer_BufferMaterial=SWMaterial;
    p->Renderer_BufferModel=SWBufferModel;p->Renderer_FlushBuffers=SWFlush;p->Renderer_SetViewport=SWViewport;
}
