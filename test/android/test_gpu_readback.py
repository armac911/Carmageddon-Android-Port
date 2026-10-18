#!/usr/bin/env python3
"""Real headless GLES regression: production readback shader and state restore.
Requires Mesa EGL (libEGL.so.1); runs without an Android device.
"""
from pathlib import Path
import subprocess,tempfile,json,os
root=Path(__file__).resolve().parents[2]
s=(root/'src/harness/renderers/gl/gl_renderer.c').read_text()
a=s.index('static void ReadbackAtCanvasResolution(void)');b=s.index('\n#endif',a)
function=s[a:b]
shader=lambda name: json.dumps((root/'src/harness/resources/gles_header.glsl').read_text()+(root/'src/harness/resources'/name).read_text())
source=r'''
#include <glad/glad.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void* eglGetProcAddress(const char*);
extern unsigned eglInitialize(void*,int*,int*);
extern unsigned eglBindAPI(unsigned);
extern unsigned eglChooseConfig(void*,const int*,void**,int,int*);
extern void* eglCreateContext(void*,void*,void*,const int*);
extern unsigned eglMakeCurrent(void*,void*,void*,void*);
static GLuint readback_program,readback_framebuffer,readback_texture,framebuffer_texture,framebuffer_id,screen_buffer_vao;
static GLuint deferred_shadow_texture;
static int pending_shadow;
static int render_width=33,render_height=25,super_factor;
static unsigned char result[33*25];
static unsigned char* screen_buffer_flip_pixels=result;
'''+function+r'''
static GLuint shader(int type,const char* text) {
 GLuint s=glCreateShader(type);glShaderSource(s,1,&text,NULL);glCompileShader(s);
 int ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(s,4096,NULL,log);puts(log);exit(1);}return s;
}
static GLuint texture(int w,int h,const void* pixels) {
 GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
 glTexImage2D(GL_TEXTURE_2D,0,GL_R8UI,w,h,0,GL_RED_INTEGER,GL_UNSIGNED_BYTE,pixels);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);return t;
}
static GLuint target(GLuint t) {
 GLuint f;glGenFramebuffers(1,&f);glBindFramebuffer(GL_FRAMEBUFFER,f);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);
 assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);return f;
}
int main(void) {
 void* (*platform)(unsigned,void*,const int*)=(void*)eglGetProcAddress("eglGetPlatformDisplayEXT");assert(platform);
 void* display=platform(0x31DD,NULL,NULL);int major,minor;assert(eglInitialize(display,&major,&minor));assert(eglBindAPI(0x30A0));
 int attrs[]={0x3040,0x40,0x3033,1,0x3038},count;void* config;assert(eglChooseConfig(display,attrs,&config,1,&count)&&count);
 int contextAttrs[]={0x3098,3,0x3038};void* context=eglCreateContext(display,config,NULL,contextAttrs);assert(context);assert(eglMakeCurrent(display,NULL,NULL,context));
 assert(gladLoadGLES2Loader((GLADloadproc)eglGetProcAddress));printf("GLES: %s\n",glGetString(GL_RENDERER));
 glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 GLuint vs=shader(GL_VERTEX_SHADER,VERTEX),fs=shader(GL_FRAGMENT_SHADER,FRAGMENT);
 readback_program=glCreateProgram();glAttachShader(readback_program,vs);glAttachShader(readback_program,fs);glLinkProgram(readback_program);
 int linked;glGetProgramiv(readback_program,GL_LINK_STATUS,&linked);assert(linked);glUseProgram(readback_program);
 glUniform1i(glGetUniformLocation(readback_program,"source_pixels"),0);
 float vertices[]={-1,-1,0, 1,-1,0, 1,1,0, -1,1,0};unsigned indices[]={0,1,3,1,2,3};GLuint vbo,ebo;
 glGenVertexArrays(1,&screen_buffer_vao);glBindVertexArray(screen_buffer_vao);
 glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
 glVertexAttribPointer(0,3,GL_FLOAT,0,3*sizeof(float),0);glEnableVertexAttribArray(0);
 glGenBuffers(1,&ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);
 readback_texture=texture(33,25,NULL);readback_framebuffer=target(readback_texture);
 GLuint dummy=texture(1,1,NULL),otherVao;glGenVertexArrays(1,&otherVao);
 for(int factor=1;factor<=3;factor++) {
  super_factor=factor;
  int w=33*factor,h=25*factor;unsigned char* pixels=malloc(w*h);
  for(int y=0;y<h;y++)for(int x=0;x<w;x++)pixels[y*w+x]=(x*7+y*19)%256;
  framebuffer_texture=texture(w,h,pixels);framebuffer_id=target(framebuffer_texture);
  glUseProgram(readback_program);glUniform1i(glGetUniformLocation(readback_program,"sample_factor"),factor);
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dummy);glActiveTexture(GL_TEXTURE4);
  glBindVertexArray(otherVao);glViewport(3,5,17,19);glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glEnable(GL_SCISSOR_TEST);glScissor(2,2,1,1);
  ReadbackAtCanvasResolution();assert(glGetError()==GL_NO_ERROR);
  for(int y=0;y<25;y++)for(int x=0;x<33;x++)assert(result[y*33+x]==pixels[(y*factor)*w+x*factor]);
  // Sub-pixmap and Y-flip select identical source samples, including zeros.
  for(int y=0;y<7;y++)for(int x=0;x<11;x++)assert(result[(25-4-7+y)*33+3+x]==pixels[((25-4-7+y)*factor)*w+(3+x)*factor]);
  GLint state,viewport[4];glGetIntegerv(GL_ACTIVE_TEXTURE,&state);assert(state==GL_TEXTURE4);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&state);assert(state==(int)otherVao);
  glGetIntegerv(GL_CURRENT_PROGRAM,&state);assert(state==(int)readback_program);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING,&state);assert(state==(int)framebuffer_id);
  glGetIntegerv(GL_VIEWPORT,viewport);assert(viewport[0]==3&&viewport[1]==5&&viewport[2]==17&&viewport[3]==19);
  assert(glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_SCISSOR_TEST));
  glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&state);assert(state==(int)dummy);
  free(pixels);
 }
 puts("PASS: exact GPU palette samples at 1x/2x/3x, sub-pixmap offsets, GL state restoration");
}
'''
source=source.replace(',VERTEX)',','+shader('framebuffer_vert.glsl')+')').replace(',FRAGMENT)',','+shader('readback_frag.glsl')+')')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(source)
 subprocess.run(['gcc','-std=c99','-I'+str(root/'lib/glad/include'),str(p/'test.c'),str(root/'lib/glad/src/glad.c'),'-l:libEGL.so.1','-ldl','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
