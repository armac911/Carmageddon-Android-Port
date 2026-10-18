#!/usr/bin/env python3
"""Check production reply handling: text bounds, stale dialogs and Cancel."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2];s=(root/'src/DETHRACE/android_jni.c').read_text()
a=s.index('static SDL_SpinLock');b=s.index('JNIEXPORT',a);source=s[a:b]
for name in ['Java_com_dethrace_android_DethraceActivity_nativeSubmitText','DRAndroidPollText']:
    pos=s.index(name+'(');a=s.rfind('\n',0,pos)+1
    if name.startswith('Java_'): a=s.rfind('JNIEXPORT',0,a)
    end=s.index('{',a)+1;depth=1
    while depth: depth+=(s[end]=='{')-(s[end]=='}');end+=1
    source+=s[a:end]+'\n'
prefix='''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int SDL_SpinLock;
static void SDL_AtomicLock(int* p){} static void SDL_AtomicUnlock(int* p){}
#define JNIEXPORT
#define JNICALL
typedef int jint; typedef int jboolean; typedef void* jobject; typedef const char* jstring;
typedef struct Jni* JNIEnv;
struct Jni {const char* (*GetStringUTFChars)(JNIEnv*,jstring,void*);void (*ReleaseStringUTFChars)(JNIEnv*,jstring,const char*);};
static const char* get(JNIEnv* e,jstring s,void* p){return s;}
static void release(JNIEnv* e,jstring s,const char* p){}
static char gCurrent_typing[110]="Original"; static int gCurrent_position;
static void DoRLBackspace(int slot){gCurrent_typing[--gCurrent_position]=0;}
static void DoRLTypeLetter(int c,int slot){gCurrent_typing[gCurrent_position++]=c;gCurrent_typing[gCurrent_position]=0;}
'''
test='''
int main(void){
 struct Jni functions={get,release};JNIEnv env=&functions;
 text_session=7;text_limit=11;
 Java_com_dethrace_android_DethraceActivity_nativeSubmitText(&env,NULL,7,"New Name 123456",1);
 assert(DRAndroidPollText(0)==1);assert(strcmp(gCurrent_typing,"New Name 12")==0);
 assert(DRAndroidPollText(0)==0);
 Java_com_dethrace_android_DethraceActivity_nativeSubmitText(&env,NULL,6,"Stale",1);
 assert(DRAndroidPollText(0)==0);
 Java_com_dethrace_android_DethraceActivity_nativeSubmitText(&env,NULL,7,"Changed",0);
 assert(DRAndroidPollText(0)==2);assert(strcmp(gCurrent_typing,"New Name 12")==0);
 Java_com_dethrace_android_DethraceActivity_nativeSubmitText(&env,NULL,7,"",1);
 assert(DRAndroidPollText(0)==1&&gCurrent_typing[0]==0);
 char long_text[201];memset(long_text,'A',200);long_text[200]=0;text_limit=100;
 Java_com_dethrace_android_DethraceActivity_nativeSubmitText(&env,NULL,7,long_text,1);
 assert(DRAndroidPollText(0)==1&&strlen(gCurrent_typing)==100);
 puts("PASS: production text reply limits, replacement, empty text, Cancel, stale dialogs, consume once");
}
'''
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c';exe=Path(d)/'test';c.write_text(prefix+source+test)
    subprocess.run(['gcc','-std=c99','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
    subprocess.run([str(exe)],env=env,check=True)
