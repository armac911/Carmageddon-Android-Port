"""Exercise the production SDL polling function with deterministic controller state."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/harness/platforms/sdl_opengl.c').read_text()
def function(text,name,prefix):
 a=text.index(prefix+' '+name+'(');b=text.index('{',a);d=1;e=b+1
 while d:
  d+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
poll=function(source,'dr_poll_controllers','static void')
normal=function((root/'src/DETHRACE/android_jni.c').read_text(),'DRAndroidNormalizeStick','int')
fixture=r'''
#include <SDL.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
struct _SDL_GameController {int id;Sint16 axes[SDL_CONTROLLER_AXIS_MAX];Uint8 buttons[SDL_CONTROLLER_BUTTON_MAX];};
static struct _SDL_GameController fake[2]={{.id=101},{.id=102}};
static int connected=1,focus=1,closed;
SDL_Window *window=(SDL_Window*)1;
static struct {int racing;} gProgram_state={1};
static int gInterface_within_race_mode;
static SDL_GameController *dr_controllers[4];
static Sint16 dr_activity_axes[4][SDL_CONTROLLER_AXIS_MAX];
static int steering,keys[SDL_NUM_SCANCODES];static unsigned actions;
static Uint8 dr_controller_keys[SDL_NUM_SCANCODES];
/* BINDINGS */
void DRAndroidSetGamepadSteering(int s){steering=s;}
void DRAndroidSetGamepadButtons(unsigned m){actions=m;}
static void dr_controller_key(SDL_Scancode s,int down){if(down)keys[s]=1;}
int SDL_NumJoysticks(void){return connected;}
SDL_bool SDL_IsGameController(int d){return d<connected?SDL_TRUE:SDL_FALSE;}
SDL_JoystickID SDL_JoystickGetDeviceInstanceID(int d){return fake[d].id;}
SDL_GameController *SDL_GameControllerOpen(int d){return &fake[d];}
void SDL_GameControllerClose(SDL_GameController *c){(void)c;closed++;}
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *c){return (SDL_Joystick*)c;}
SDL_JoystickID SDL_JoystickInstanceID(SDL_Joystick *j){return ((SDL_GameController*)j)->id;}
SDL_bool SDL_GameControllerGetAttached(SDL_GameController *c){return c->id==101?connected>=1:connected>=2;}
void SDL_GameControllerUpdate(void){}
Sint16 SDL_GameControllerGetAxis(SDL_GameController *c,SDL_GameControllerAxis a){return c->axes[a];}
Uint8 SDL_GameControllerGetButton(SDL_GameController *c,SDL_GameControllerButton b){return c->buttons[b];}
Uint32 SDL_GetWindowFlags(SDL_Window *w){(void)w;return focus?SDL_WINDOW_INPUT_FOCUS:0;}
/* NORMALIZE */
/* POLL */
static void poll(void){memset(keys,0,sizeof(keys));dr_poll_controllers();}
static void clear(void){memset(fake[0].axes,0,sizeof(fake[0].axes));memset(fake[0].buttons,0,sizeof(fake[0].buttons));}
int main(void){
 unsetenv("DETHRACE_CONTROLLER_BINDINGS");unsetenv("DETHRACE_CONTROLLER_STEERING");
 poll();assert(actions==0&&steering==0);
 fake[0].axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]=4001;poll();assert(actions==1);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_X]=1;poll();assert(actions==1);
 clear();fake[0].axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT]=4001;poll();assert(actions==2);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]=1;poll();assert(actions==4);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]=1;poll();assert(actions==8);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_Y]=1;poll();assert(actions==16);
 fake[0].buttons[SDL_CONTROLLER_BUTTON_X]=1;
 fake[0].buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER]=1;
 fake[0].buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]=1;poll();assert(actions==29);
 clear();fake[0].axes[SDL_CONTROLLER_AXIS_LEFTX]=-20000;poll();assert(steering<0&&steering>-65535);
 fake[0].buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]=1;poll();assert(steering==65535);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT]=1;poll();assert(steering==-65535);
 clear();fake[0].axes[SDL_CONTROLLER_AXIS_LEFTY]=-32767;fake[0].buttons[SDL_CONTROLLER_BUTTON_DPAD_UP]=1;poll();assert(actions==0);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_BACK]=1;poll();assert(actions==64);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_RIGHTSTICK]=1;poll();assert(actions==32);
 clear();fake[0].axes[SDL_CONTROLLER_AXIS_RIGHTY]=-17000;poll();assert(actions==256);
 fake[0].axes[SDL_CONTROLLER_AXIS_RIGHTY]=-9000;poll();assert(actions==256);
 fake[0].axes[SDL_CONTROLLER_AXIS_RIGHTY]=-5000;poll();assert(actions==0);
 fake[0].axes[SDL_CONTROLLER_AXIS_RIGHTY]=17000;poll();assert(actions==512);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_A]=1;fake[0].buttons[SDL_CONTROLLER_BUTTON_B]=1;poll();assert(actions==0);
 gInterface_within_race_mode=1;fake[0].buttons[SDL_CONTROLLER_BUTTON_DPAD_UP]=1;poll();
 assert(actions==0&&steering==100000&&keys[SDL_SCANCODE_UP]&&keys[SDL_SCANCODE_RETURN]&&keys[SDL_SCANCODE_ESCAPE]);
 gInterface_within_race_mode=0;clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_START]=1;poll();assert(keys[SDL_SCANCODE_ESCAPE]);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_Y]=1;focus=0;poll();assert(actions==0&&steering==100000);
 focus=1;poll();assert(actions==16);
 connected=0;poll();assert(actions==0&&steering==100000&&closed==1);
 connected=2;clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_X]=1;fake[1].buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]=1;poll();assert(actions==9);
 // Remap throttle to B, disable secondary throttle and change steering input.
 connected=1;clear();setenv("DETHRACE_CONTROLLER_BINDINGS","1,109,9,10,3,8,4,6,106,107,-1,0,2",1);
 setenv("DETHRACE_CONTROLLER_STEERING","1",1);dr_bindings_loaded=0;memset(dr_input_latches,0,sizeof(dr_input_latches));
 fake[0].buttons[SDL_CONTROLLER_BUTTON_B]=1;poll();assert(actions==1);
 clear();fake[0].buttons[SDL_CONTROLLER_BUTTON_X]=1;poll();assert(actions==0);
 fake[0].buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]=1;poll();assert(steering==0);
 fake[0].axes[SDL_CONTROLLER_AXIS_LEFTX]=32767;poll();assert(steering==65535);
 gInterface_within_race_mode=1;poll();assert(keys[SDL_SCANCODE_ESCAPE]&&actions==0);
 puts("PASS production SDL mapping, Select map, R3 camera, gear-axis hysteresis, remapping and disabled bindings: RT/X, LT, LB, RB, Y, steering, chords, menus, pause, focus loss, disconnect/reconnect and multiple controllers");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(fixture.replace('/* BINDINGS */',(root/'src/harness/platforms/android_controller.h').read_text()).replace('/* NORMALIZE */',normal).replace('/* POLL */',poll))
 subprocess.run(['cc','-D_POSIX_C_SOURCE=200809L','-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'android/app/jni/SDL/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
