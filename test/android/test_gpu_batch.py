#!/usr/bin/env python3
"""Headless GLES: real production flush/batch/blend routines vs eager readback."""
from pathlib import Path
import runpy, json, subprocess, tempfile, os
root=Path(__file__).resolve().parents[2]
base=runpy.run_path(str(root/'test/android/test_gpu_readback.py'))
s=(root/'src/harness/renderers/gl/gl_renderer.c').read_text()
def func(name):
 a=s.index('void '+name+'('); b=s.index('{',a);depth=1;i=b+1
 while depth:
  if s[i]=='{':depth+=1
  if s[i]=='}':depth-=1
  i+=1
 return s[a:i]
extra=r'''
#define __ANDROID__ 1
#include <stdint.h>
#define DRProfileClock() 0
#define DRProfileTime(a,b) ((void)0)
#define DRP_READS 1
#define DRP_BYTES 2
static int measured_reads;
#define DRProfileCount(a,b) do {if((a)==DRP_READS) measured_reads+=(b);}while(0)
#define CHECK_GL_ERROR(x) assert(glGetError()==GL_NO_ERROR)
typedef struct {void*pixels;int width,height,row_bytes,base_x,base_y;void*stored;} br_pixelmap;
typedef struct {GLuint id;} tStored_pixelmap;
typedef enum {eFlush_color_buffer,eFlush_all} tRenderer_flush_type;
static int super_width,super_height,dirty_buffers,shadow_batch,mirror_scene;
static unsigned flush_counter=1,colourbuffer_upload_counter;
static GLuint current_colourbuffer_texture;
static unsigned char cpu_pixels[33*25],snapshot_data[33*25];
static unsigned short depth_data[33*25];
static uint8_t *snapshot_pixels=snapshot_data;
static br_pixelmap colour,depth,*last_colour_buffer=&colour,*last_depth_buffer=&depth;
br_pixelmap *gBack_screen=&colour;
void GLRenderer_FlushBuffer(tRenderer_flush_type flush_type);
'''
extra+='\n'+(root/'src/harness/renderers/gl/indexed_copy.h').read_text()+'\n'+func('GLRenderer_SetBlendTable')+'\n'+func('GLRenderer_FlushBuffer')+'\n'+func('GLRenderer_FlushBuffers')+'\n'+func('DRBeginShadowBatch')+'\n'+func('DREndShadowBatch')+'\n'+func('DRDeferShadowBatch')
extra+=r'''
static GLuint scene_program,blend_id;
static void prepare_scene(int factor) {
 super_factor=factor;super_width=render_width*factor;super_height=render_height*factor;
 framebuffer_texture=texture(super_width,super_height,NULL);framebuffer_id=target(framebuffer_texture);
 GLuint depthtex;glGenTextures(1,&depthtex);glBindTexture(GL_TEXTURE_2D,depthtex);
 glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,super_width,super_height,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,NULL);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthtex,0);
 assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
 current_colourbuffer_texture=texture(super_width,super_height,NULL);
 deferred_shadow_texture=texture(super_width,super_height,NULL);
 pending_shadow=0;
 glUseProgram(readback_program);glUniform1i(glGetUniformLocation(readback_program,"sample_factor"),factor);
 colour=(br_pixelmap){cpu_pixels,33,25,33,0,0,NULL};depth=(br_pixelmap){depth_data,33,25,66,0,0,NULL};
 memset(cpu_pixels,5,sizeof(cpu_pixels));memset(snapshot_data,0,sizeof(snapshot_data));
 glActiveTexture(GL_TEXTURE0);glUseProgram(scene_program);glBindVertexArray(screen_buffer_vao);
 glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_id);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);glDisable(GL_CULL_FACE);
 const GLuint zero[4]={0};glClearBufferuiv(GL_COLOR,0,zero);
 dirty_buffers=shadow_batch=0;flush_counter=1;colourbuffer_upload_counter=0;measured_reads=0;
 glUniform1i(glGetUniformLocation(scene_program,"u_material_palette"),1);
 glUniform1i(glGetUniformLocation(scene_program,"u_filter_lut"),7);
 glUniform1i(glGetUniformLocation(scene_program,"u_texture_filtering"),0);
 glUniform1i(glGetUniformLocation(scene_program,"u_colour_buffer"),4);
 glUniform1i(glGetUniformLocation(scene_program,"u_material_blend_table"),3);
 glUniform1ui(glGetUniformLocation(scene_program,"u_material_texture_enabled"),0);
 glUniform1ui(glGetUniformLocation(scene_program,"u_material_flags"),0);
 glUniform1i(glGetUniformLocation(scene_program,"u_clip_plane_count"),0);
}
static void rect(int x,int y,int w,int h,unsigned index,int blend) {
 glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_id);glUseProgram(scene_program);glBindVertexArray(screen_buffer_vao);
 glViewport(x*super_factor,y*super_factor,w*super_factor,h*super_factor);
 glUniform1f(glGetUniformLocation(scene_program,"zvalue"),(index%7)/7.f*1.5f-.75f);
 glDepthMask(blend?GL_FALSE:GL_TRUE);
 glUniform1ui(glGetUniformLocation(scene_program,"u_material_index_base"),index);
 glUniform1ui(glGetUniformLocation(scene_program,"u_material_blend_enabled"),blend);
 glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);dirty_buffers=1;assert(glGetError()==GL_NO_ERROR);
}
static void exercise(void) {
 unsigned char reference[33*25];
 // An empty batch must be safe before the first BeginScene / pixelmap binding.
 last_colour_buffer=last_depth_buffer=NULL;dirty_buffers=0;DRBeginShadowBatch();DREndShadowBatch();
 last_colour_buffer=&colour;last_depth_buffer=&depth;
 for(int factor=1;factor<=3;factor++) {
  for(int batched=0;batched<=1;batched++) {
   prepare_scene(factor);if(batched)DRBeginShadowBatch();
   for(int j=0;j<6;j++) {
    rect(2+j,2+j,12,9,30+j*7,0);GLRenderer_FlushBuffers();
    // Each real shadow EndScene binds framebuffer 0; next scene rebinds it.
    glBindFramebuffer(GL_FRAMEBUFFER,0);
   }
   if(batched)DREndShadowBatch();
   assert(measured_reads==(batched?1:6));
   if(!batched)memcpy(reference,cpu_pixels,sizeof(reference));
   else assert(!memcmp(reference,cpu_pixels,sizeof(reference)));
  }

  // 0.1.13 shadow read + main read versus one merged read. Include a
  // blended object: its snapshot must NOT contain the deferred shadow layer.
  for(int merged=0;merged<=1;merged++) {
   prepare_scene(factor);DRBeginShadowBatch();
   rect(1,2,17,14,37,0);rect(15,10,12,11,52,0);GLRenderer_FlushBuffers();
   if(merged) DRDeferShadowBatch(); else DREndShadowBatch();
   rect(4,4,9,8,68,0);
   tStored_pixelmap stored={blend_id};br_pixelmap table={0};table.stored=&stored;
   GLRenderer_SetBlendTable(&table);rect(8,5,10,9,135,1);
   GLRenderer_FlushBuffers();
   assert(measured_reads==(merged?1:2));assert(!pending_shadow);
   if(!merged) memcpy(reference,cpu_pixels,sizeof(reference));
   else assert(!memcmp(reference,cpu_pixels,sizeof(reference)));
   cpu_pixels[0]=207;rect(18,17,3,3,93,0);GLRenderer_FlushBuffers();assert(cpu_pixels[0]==207);
  }
  // No main geometry: pending shadows must still reach the CPU consumer.
  prepare_scene(factor);DRBeginShadowBatch();rect(3,3,8,8,72,0);DRDeferShadowBatch();
  assert(!dirty_buffers && pending_shadow);GLRenderer_FlushBuffers();assert(measured_reads==1 && !pending_shadow);
  for(int deferred=0;deferred<=1;deferred++) {
   prepare_scene(factor);
   rect(0,0,33,25,23,0);rect(3,3,12,9,73,0);
   tStored_pixelmap stored={blend_id};br_pixelmap table={0};table.stored=&stored;
   GLRenderer_SetBlendTable(&table);
   // Reference emulates 0.1.12's synchronous flush after the GPU snapshot.
   if(!deferred)GLRenderer_FlushBuffer(eFlush_color_buffer);
   rect(4,2,17,10,112,1);rect(9,6,12,10,159,1);
   GLRenderer_FlushBuffers();assert(measured_reads==(deferred?1:2));
   if(!deferred)memcpy(reference,cpu_pixels,sizeof(reference));
   else assert(!memcmp(reference,cpu_pixels,sizeof(reference)));
   // CPU effect between actual flush boundaries must survive transparent gaps.
   cpu_pixels[0]=201;rect(8,8,5,5,94,0);GLRenderer_FlushBuffers();assert(cpu_pixels[0]==201);
   assert(glGetError()==GL_NO_ERROR);
  }
 }
 puts("PASS: production deferred shadow+main 2->1 with exact blended output; shadow batching 6->1 readbacks; blend snapshot 2->1; exact palette output at 1x/2x/3x, overlaps, empty batch and CPU-effect preservation");
}
'''
source=base['source']; idx=source.index('int main(void)');source=source[:idx]+extra+'\n'+source[idx:]
vertex='#version 300 es\nlayout(location=0) in vec3 p;uniform float zvalue; out vec3 v_frag_pos;out vec3 v_normal;out vec2 v_tex_coord;out float v_color;void main(){gl_Position=vec4(p.xy,zvalue,1);v_frag_pos=p;v_normal=vec3(0,0,1);v_tex_coord=vec2(0);v_color=0.;}'
frag=(root/'src/harness/resources/gles_header.glsl').read_text()+(root/'src/harness/resources/3d_frag.glsl').read_text()
setup='''
 scene_program=glCreateProgram();
 glAttachShader(scene_program,shader(GL_VERTEX_SHADER,VERTEX));glAttachShader(scene_program,shader(GL_FRAGMENT_SHADER,FRAGMENT));glLinkProgram(scene_program);
 GLint good;glGetProgramiv(scene_program,GL_LINK_STATUS,&good);assert(good);
 unsigned char table[256*256];for(int y=0;y<256;y++)for(int x=0;x<256;x++)table[y*256+x]=(x*3+y*5)%255+1;
 blend_id=texture(256,256,table);exercise();
'''.replace(',VERTEX)',','+json.dumps(vertex)+')').replace(',FRAGMENT)',','+json.dumps(frag)+')')
source=source.replace(' puts("PASS: exact GPU',setup+' puts("PASS: exact GPU')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(source)
 subprocess.run(['gcc','-std=c99','-I'+str(root/'lib/glad/include'),str(p/'test.c'),str(root/'lib/glad/src/glad.c'),'-l:libEGL.so.1','-ldl','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
