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
       'Java_com_dethrace_android_DethraceActivity_nativeTilt',
       'DRAndroidUpdateTouch', 'DRAndroidTouchKey', 'DRAndroidSetTiltWithController', 'DRAndroidSteering', 'store_max',
       'DRAndroidImpactStrength', 'DRAndroidImpact', 'DRAndroidTakeControllerRumble',
       'Java_com_dethrace_android_DethraceActivity_nativeTakeImpact']
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
#include <math.h>
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
#define tilt(axis,on) Java_com_dethrace_android_DethraceActivity_nativeTilt(NULL,NULL,axis,on)
int main(void) {
 // Stick range, deadzone, asymmetric signed limits, and touch priority.
 assert(DRAndroidNormalizeStick(-32768)==-65535);
 assert(DRAndroidNormalizeStick(32767)==65535);
 assert(DRAndroidNormalizeStick(8000)==0 && DRAndroidNormalizeStick(-8000)==0);
 int partial=DRAndroidNormalizeStick(20000);
 assert(partial>30000 && partial<33000);
 assert(DRAndroidNormalizeStick(-20000)==-partial);
 DRAndroidSetGamepadSteering(partial); assert(DRAndroidSteering()==partial);
 touch(0,-12000,1); assert(DRAndroidSteering()==-12000);
 reset(); assert(DRAndroidSteering()==partial);
 DRAndroidSetGamepadSteering(100000); assert(DRAndroidSteering()==100000);
 setenv("DETHRACE_STEERING","digital",1); digital_steering=-1; assert(DRAndroidDigitalSteering());
 setenv("DETHRACE_STEERING","analog",1); assert(DRAndroidDigitalSteering()); /* cached for the session */
 digital_steering=-1; assert(!DRAndroidDigitalSteering());
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
 // Tilt: after a finger on the pad and a controller in use, before an idle controller.
 gInterface_within_race_mode=0; reset(); DRAndroidUpdateTouch();
 DRAndroidSetGamepadSteering(100000); using_gamepad=0;
 tilt(-30000,1); assert(DRAndroidSteering()==-30000);
 tilt(-999999,1); assert(DRAndroidSteering()==-65535); tilt(-30000,1);
 touch(0,5000,1); assert(DRAndroidSteering()==5000); reset();
 DRAndroidSetGamepadSteering(1234); using_gamepad=1; assert(DRAndroidSteering()==1234-30000); // combined (default)
 DRAndroidSetGamepadSteering(-50000); assert(DRAndroidSteering()==-65535);                 // clamped
 DRAndroidSetTiltWithController(0); assert(DRAndroidSteering()==-50000);                  // controller only
 DRAndroidSetTiltWithController(1); DRAndroidSetGamepadSteering(1234);
 using_gamepad=0; assert(DRAndroidSteering()==-30000);
 DRAndroidSetGamepadSteering(100000); using_gamepad=1; assert(DRAndroidSteering()==-30000);
 using_gamepad=0; DRAndroidSetGamepadSteering(1234); tilt(0,0); assert(DRAndroidSteering()==1234);
 DRAndroidSetGamepadSteering(100000);
 // Impact strength follows the velocity change (m/s) of a collision frame.
 assert(DRAndroidImpactStrength(1.0f)==0 && DRAndroidImpactStrength(1.5f)==150);
 assert(DRAndroidImpactStrength(16.5f)==1000 && DRAndroidImpactStrength(40.f)==1000);
 assert(DRAndroidImpactStrength(5.f)<DRAndroidImpactStrength(10.f));
 float still[3]={0,0,0}, fast[3]={20,0,0}, slowed[3]={8,0,0}, scrape1[3]={5,0,0}, scrape2[3]={2,0,0};
 #define take() Java_com_dethrace_android_DethraceActivity_nativeTakeImpact(NULL,NULL)
 ticks+=1000; DRAndroidImpact(1,fast); assert(!impact);            // no reference after a gap
 ticks+=16; DRAndroidImpact(0,fast); ticks+=16; DRAndroidImpact(1,slowed);
 int hit=take(); assert(hit==DRAndroidImpactStrength(12.f)); assert(!impact);
 assert(DRAndroidTakeControllerRumble()==hit); assert(!controller_rumble);
 ticks+=16; DRAndroidImpact(0,still); assert(!impact);             // hard braking, no collision
 ticks+=16; DRAndroidImpact(1,still); assert(!impact);             // contact without a speed change
 ticks+=200; DRAndroidImpact(1,scrape1); int first=take(); assert(first==DRAndroidImpactStrength(5.f));
 ticks+=16; DRAndroidImpact(1,still); assert(take()==0);           // same-size scrape within 120 ms: suppressed
 ticks+=16; DRAndroidImpact(1,fast); assert(take()==1000);         // a harder hit always gets through
 ticks+=200; DRAndroidImpact(1,still); assert(take()==1000);       // after the cooldown any hit pulses again
 ticks+=200; DRAndroidImpact(1,scrape2); ticks+=200; DRAndroidImpact(1,(float[]){14,0,0});
 assert(take()==DRAndroidImpactStrength(12.f));                    // strongest pending hit wins
 (void)slowed;
 puts("PASS: stick normalization, input priority, tilt priority, tilt + controller combine, steering modes, FPS/pause sampling, combined controls, partial steering, keymap, quick taps, reset, menu transition, graded impacts and scrape cooldown");
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c'; exe=Path(d)/'test'; c.write_text(prefix+source+test)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Wno-unused-parameter',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
