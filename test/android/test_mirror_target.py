"""Real GLES: production mirror target swapping/readback preserves main colour/depth and snapshot."""
from pathlib import Path
import runpy,subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
base=runpy.run_path(str(root/'test/android/test_gpu_batch.py'))
s=base['s'];a=s.index('void SwapMirrorTarget(');b=s.index('\n}',a)+2
extra='\nstatic GLuint mirror_fbo,mirror_colour,mirror_depth,depth_texture;\n'+s[a:b]+'\n'+base['func']('GLRenderer_EndScene')+r'''
static void mirror_exercise(void) {
 for(int factor=1;factor<=3;factor++) {
  prepare_scene(factor);
  rect(0,0,33,25,19,0);dirty_buffers=0; // settled main GPU scene
  memset(snapshot_data,71,sizeof(snapshot_data));
  GLuint main_fbo=framebuffer_id,main_colour=framebuffer_texture;GLint depth_name;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&depth_name);depth_texture=depth_name;GLuint main_depth=depth_texture;
  mirror_colour=texture(super_width,super_height,NULL);mirror_fbo=target(mirror_colour);
  glGenTextures(1,&mirror_depth);glBindTexture(GL_TEXTURE_2D,mirror_depth);
  glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,super_width,super_height,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,NULL);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,mirror_depth,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  SwapMirrorTarget();mirror_scene=1;
  assert(framebuffer_id!=main_fbo && framebuffer_texture!=main_colour && depth_texture!=main_depth);
  unsigned char mirror_pixels[9*5]={0};unsigned short mirror_z[9*5];
  br_pixelmap pm={0},zm={0};pm.pixels=mirror_pixels;pm.width=9;pm.height=5;pm.row_bytes=9;
  zm.pixels=mirror_z;zm.width=9;zm.height=5;zm.row_bytes=18;
  last_colour_buffer=&pm;last_depth_buffer=&zm;
  static const GLuint zero[4]={0};glClearBufferuiv(GL_COLOR,0,zero);glClear(GL_DEPTH_BUFFER_BIT);
  rect(0,20,9,5,87,0);GLRenderer_FlushBuffers();
  for(int i=0;i<45;i++)assert(mirror_pixels[i]==87);
  for(int i=0;i<33*25;i++)assert(snapshot_data[i]==71);
  glFrontFace(GL_CW);glDepthMask(GL_FALSE);GLRenderer_EndScene();
  GLint winding;glGetIntegerv(GL_FRONT_FACE,&winding);assert(winding==GL_CCW);assert(!mirror_scene);
  assert(framebuffer_id==main_fbo && framebuffer_texture==main_colour && depth_texture==main_depth);
  glBindFramebuffer(GL_FRAMEBUFFER,main_fbo);unsigned char pixel=0;
  glReadPixels(0,0,1,1,GL_RED_INTEGER,GL_UNSIGNED_BYTE,&pixel);assert(pixel==19);
  last_colour_buffer=&colour;last_depth_buffer=&depth;
 }
 puts("PASS mirror target isolation, bounded sub-pixmap readback, main image/depth and snapshot preservation at 1x/2x/3x");
}
'''
source=base['source'];i=source.index('int main(void)');source=source[:i]+extra+'\n'+source[i:]
source=source.replace('exercise();','exercise();mirror_exercise();')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run(['gcc','-std=c99','-I'+str(root/'lib/glad/include'),str(p/'test.c'),str(root/'lib/glad/src/glad.c'),'-l:libEGL.so.1','-ldl','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
