#!/usr/bin/env python3
"""Exercise the production touch bridge with deterministic time and atomics."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[2]
s = (root/'src/DETHRACE/android_jni.c').read_text()
a=s.index('static SDL_atomic_t'); b=s.index('static SDL_SpinLock')
source=s[a:b]
names=['Java_com_dethrace_android_DethraceActivity_nativeTouch',
       'Java_com_dethrace_android_DethraceActivity_nativeResetTouch',
       'DRAndroidUpdateTouch', 'DRAndroidTouchKey', 'DRAndroidSteering', 'DRAndroidFilterSteering', 'DRAndroidPedals', 'DRAndroidImpact']
for name in names:
    start=s.index(name+'('); start=s.rfind('\n',0,start)+1
    if name.startswith('Java_'): start=s.rfind('JNIEXPORT',0,start)
    else: start=s.rfind('\n',0,s.index(name+'('))+1
    brace=s.index('{',start); end=brace+1; depth=1
    while depth:
        depth += (s[end]=='{')-(s[end]=='}'); end+=1
    source += s[start:end]+'\n'
prefix='''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "android_control_tuning.h"
typedef int SDL_atomic_t;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef void JNIEnv;
typedef void* jobject;
typedef int jint;
typedef int jboolean;
#define JNIEXPORT
#define JNICALL
static int SDL_AtomicSet(int* a,int b) { int old=*a; *a=b; return old; }
static int SDL_AtomicGet(int* a) {return *a;}
static int SDL_AtomicCAS(int* a,int old,int n) { if (*a!=old) return 0; *a=n; return 1; }
static unsigned ticks=1000;
static unsigned SDL_GetTicks(void) {return ticks;}
static struct {int racing;} gProgram_state={1};
static int gInterface_within_race_mode, gKey_mapping[67];
#define KEYMAP_REPAIR 44
#define KEYMAP_RECOVERY 59
#define KEYMAP_COCKPIT 45
#define KEYMAP_MAP 56
#define KEY_ESCAPE 63
'''
test='''
#define touch(mask,axis,on) Java_com_dethrace_android_DethraceActivity_nativeTouch(NULL,NULL,mask,axis,on)
#define reset() Java_com_dethrace_android_DethraceActivity_nativeResetTouch(NULL,NULL)
int main(void) {
 // Production smoothing: equal held input gives equal results at 30/60/120 FPS.
 float result[3];
 for(int rate=0;rate<3;rate++) {
   int frames=rate==0?6:rate==1?12:24;
   float value=0;
   for(int i=0;i<frames;i++)value=dr_control_slew(value,65535,(i+1)*200/frames-i*200/frames,250,180);
   result[rate]=value;
 }
 assert(fabsf(result[0]-52428)<.1f&&fabsf(result[0]-result[1])<.1f&&fabsf(result[1]-result[2])<.1f);
 float one=dr_control_slew(50000,-65535,150,250,180), many=50000;
 for(int i=0;i<15;i++)many=dr_control_slew(many,-65535,10,250,180);
 assert(fabsf(one-many)<.1f);
 assert(dr_control_slew(65535,0,180,250,180)==0);
 assert(dr_control_slew(0,65535,0,250,180)==0);
 assert(dr_control_slew(0,65535,1,0,0)==65535);
 assert(dr_control_axis(-32768,32767,50,25)==-65535);
 assert(dr_control_axis(16000,32767,50,25)==0);
 setenv("DETHRACE_INPUT_STICK_DEADZONE","invalid",1);dr_control_tuning(1);
 assert(dr_control_tuning(0)->stick_deadzone==24);
 unsetenv("DETHRACE_INPUT_STICK_DEADZONE");
 DRAndroidInitControlTuning();
 int a,b;
 DRAndroidSetGamepadSteering(65535);
 assert(DRAndroidFilterSteering(65535,25)>0&&DRAndroidFilterSteering(65535,25)<20000);
 assert(DRAndroidFilterSteering(100000,16)==100000);
 assert(DRAndroidFilterSteering(65535,25)<7000); // no old lock after disconnect
 DRAndroidFilterSteering(100000,16);
 touch(0,4000,1);assert(DRAndroidFilterSteering(DRAndroidSteering(),16)==0);
 reset();DRAndroidUpdateTouch();
 DRAndroidSetGamepadPedals(32768,0);
 assert(DRAndroidPedals(&a,&b,60)&&a>30000&&a<32769&&b==-1);
 DRAndroidSetGamepadPedals(0,32768);
 assert(DRAndroidPedals(&a,&b,60)&&a==-1&&b>30000); // throttle releases immediately
 DRAndroidSetGamepadPedals(-1,-1);assert(!DRAndroidPedals(&a,&b,60));
 touch(1,0,0);DRAndroidUpdateTouch();
 assert(DRAndroidPedals(&a,&b,30)&&a>16000&&a<17000); // touch/button ramp
 touch(0,0,0);DRAndroidUpdateTouch();
 assert(DRAndroidPedals(&a,&b,30)&&a==-1&&b==-1); // release bypasses action minimum-hold
 ticks+=100;DRAndroidUpdateTouch();assert(!DRAndroidPedals(&a,&b,30));
 touch(1,0,0);DRAndroidUpdateTouch();gInterface_within_race_mode=1;DRAndroidUpdateTouch();
 assert(!DRAndroidPedals(&a,&b,30));gInterface_within_race_mode=0;
 // Releasing touch steering returns smoothly even with no controller attached.
 DRAndroidInitControlTuning();
 touch(0,65535,1);assert(DRAndroidFilterSteering(DRAndroidSteering(),250)==65535);
 touch(0,0,0);int released=DRAndroidFilterSteering(DRAndroidSteering(),90);
 assert(released>32000&&released<33000);
 assert(DRAndroidFilterSteering(DRAndroidSteering(),90)==0);
 assert(DRAndroidFilterSteering(DRAndroidSteering(),16)==100000);
 DRAndroidInitControlTuning();
 // Stick range, deadzone, asymmetric signed limits, and touch priority.
 assert(DRAndroidNormalizeStick(-32768)==-65535);
 assert(DRAndroidNormalizeStick(32767)==65535);
 assert(DRAndroidNormalizeStick(7864)==0 && DRAndroidNormalizeStick(-7864)==0);
 int partial=DRAndroidNormalizeStick(20000);
 assert(partial>27000 && partial<29000);
 assert(DRAndroidNormalizeStick(-20000)==-partial);
 DRAndroidSetGamepadSteering(partial); assert(DRAndroidSteering()==partial);
 touch(0,-12000,1); assert(DRAndroidSteering()==-12000);
 reset(); assert(DRAndroidSteering()==partial);
 DRAndroidSetGamepadSteering(100000); assert(DRAndroidSteering()==100000);
 setenv("DETHRACE_STEERING","digital",1); assert(DRAndroidDigitalSteering());
 setenv("DETHRACE_STEERING","analog",1); assert(!DRAndroidDigitalSteering());
 // Count presented frames, expire stale samples, reset after suspension.
 ticks=5000; DRAndroidFramePresented();
 for(int i=0;i<60;i++){ticks+=17;DRAndroidFramePresented();}
 assert(Java_com_dethrace_android_DethraceActivity_nativeFpsTenths(NULL,NULL)==588);
 ticks+=2100; assert(Java_com_dethrace_android_DethraceActivity_nativeFpsTenths(NULL,NULL)==0);
 DRAndroidFramePresented(); assert(measured_fps==0);
 for(int i=0;i<50;i++){ticks+=20;DRAndroidFramePresented();}
 assert(Java_com_dethrace_android_DethraceActivity_nativeFpsTenths(NULL,NULL)==500);
 ticks=1000;
 // New input switches modes; stale queued controller input cannot undo touch.
 assert(!Java_com_dethrace_android_DethraceActivity_nativeUsingGamepad(NULL,NULL));
 DRAndroidGamepadActivity(999); assert(using_gamepad);
 Java_com_dethrace_android_DethraceActivity_nativeScreenTouched(NULL,NULL);
 assert(!using_gamepad);
 DRAndroidGamepadActivity(999); assert(!using_gamepad);
 DRAndroidGamepadActivity(1000); assert(!using_gamepad);
 DRAndroidGamepadActivity(1001); assert(using_gamepad);
 ticks=1002; Java_com_dethrace_android_DethraceActivity_nativeScreenTouched(NULL,NULL);
 assert(!using_gamepad);
 for(int i=0;i<67;i++) gKey_mapping[i]=i;
 // Manual gear pulse: consume once, no repeat while held; release and menus clear.
 DRAndroidSetGamepadButtons(256);assert(DRAndroidTakeGearChange()==1);assert(DRAndroidTakeGearChange()==0);
 DRAndroidSetGamepadButtons(256);assert(DRAndroidTakeGearChange()==0);
 DRAndroidSetGamepadButtons(0);DRAndroidSetGamepadButtons(512);assert(DRAndroidTakeGearChange()==-1);
 DRAndroidSetGamepadButtons(0);DRAndroidSetGamepadButtons(256);DRAndroidSetGamepadButtons(0);assert(DRAndroidTakeGearChange()==0);
 DRAndroidSetGamepadButtons(512);gInterface_within_race_mode=1;DRAndroidSetGamepadButtons(0);assert(DRAndroidTakeGearChange()==0);gInterface_within_race_mode=0;
 DRAndroidSetGamepadButtons(32|64);assert(DRAndroidTouchKey(45)&&DRAndroidTouchKey(56));DRAndroidSetGamepadButtons(0);
 // Gamepad action chords combine with touch and follow custom loaded bindings.
 DRAndroidSetGamepadButtons(4|8|16);touch(1,0,0);DRAndroidUpdateTouch();
 assert(DRAndroidTouchKey(48)&&DRAndroidTouchKey(53)&&DRAndroidTouchKey(44)&&DRAndroidTouchKey(59));
 reset();DRAndroidUpdateTouch();assert(!DRAndroidTouchKey(48)&&DRAndroidTouchKey(44));
 gKey_mapping[44]=103;assert(DRAndroidTouchKey(103)&&!DRAndroidTouchKey(44));
 DRAndroidSetGamepadButtons(0);assert(!DRAndroidTouchKey(103));gKey_mapping[44]=44;
 touch(1|8,32768,1); DRAndroidUpdateTouch();
 assert(DRAndroidTouchKey(48) && DRAndroidTouchKey(44));
 assert(DRAndroidSteering()==32768);
 ticks+=100; touch(1,0,0); DRAndroidUpdateTouch();
 assert(DRAndroidTouchKey(48) && !DRAndroidTouchKey(44));
 assert(DRAndroidSteering()==100000);
 reset(); DRAndroidUpdateTouch(); assert(active_mask==0);
 // Press and release between polls must register exactly one short pulse.
 touch(16,0,0); touch(0,0,0); DRAndroidUpdateTouch(); assert(DRAndroidTouchKey(59));
 ticks+=81; DRAndroidUpdateTouch(); assert(!active_mask);
 touch(128,0,0); DRAndroidUpdateTouch(); assert(DRAndroidTouchKey(KEY_ESCAPE));
 reset(); DRAndroidUpdateTouch(); assert(!active_mask);
 // Custom game keymap and menu transition.
 gKey_mapping[48]=101; touch(1,-999999,1); DRAndroidUpdateTouch();
 assert(DRAndroidTouchKey(101) && !DRAndroidTouchKey(48)); assert(DRAndroidSteering()==-65535);
 gInterface_within_race_mode=1; DRAndroidUpdateTouch();
 assert(!active_mask && DRAndroidSteering()==100000);
 DRAndroidImpact(1,20); assert(SDL_AtomicSet(&impact,0)==1);
 ticks+=100; DRAndroidImpact(1,20); assert(!impact);
 DRAndroidImpact(0,20); DRAndroidImpact(1,20); assert(!impact);
 ticks+=600; DRAndroidImpact(0,20); DRAndroidImpact(1,20); assert(impact);
 puts("PASS: stick normalization, input priority, steering modes, FPS/pause sampling, combined controls, partial steering, keymap, quick taps, reset, menu transition, impact cooldown");
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c'; exe=Path(d)/'test'; c.write_text(prefix+source+test)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Wno-unused-parameter','-I'+str(root/'src/DETHRACE'),str(c),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
