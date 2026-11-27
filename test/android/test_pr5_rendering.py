"""PR5 production aspect selection, native cockpit composition and offset GPU readback."""
from pathlib import Path
import subprocess,tempfile,os,runpy
root=Path(__file__).resolve().parents[2]
def function(text,name,prefix):
 a=text.index(prefix+' '+name+'(');b=text.index('{',a);d=1;e=b+1
 while d:d+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[a:e]
w=(root/'src/harness/platforms/android_widescreen.c').read_text();w=w[w.index('/* Widest canvas'):w.rindex('#endif')]
g=(root/'src/DETHRACE/common/graphics.c').read_text()
assert 'ExtendStripImageEdges' not in g
edge=function(g,'SkipLines','tS8*')+'\n'+function(g,'CopyStripImage','void')
source=r'''
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define DR_ANDROID_WIDE_HEIGHT 486
#define LOG_INFO(...) ((void)0)
#define LOG_TRACE(...) ((void)0)
#define LOG_TRACE8(...) ((void)0)
typedef int16_t br_int_16;typedef uint16_t br_uint_16;
static struct {int total_width,total_height,phys_width,phys_height,row_bytes;} gGraf_specs[2];
static struct {int width,height;} gGraf_data[2];
typedef int8_t tS8;typedef uint8_t tU8;
typedef struct {int width,height,row_bytes;void* pixels;} br_pixelmap;
'''+w+'\n'+edge+r'''
int main(void) {
 int width,height;
 unsetenv("DETHRACE_ASPECT");setenv("DETHRACE_WIDESCREEN_16_9","1",1);
 width=640;height=480;DRAndroid_ConfigureAspect(&width,&height,0,0);assert(width==864&&height==486);
 setenv("DETHRACE_ASPECT","classic",1);DRAndroid_ConfigureAspect(&width,&height,2712,1220);assert(width==640&&height==480);
 setenv("DETHRACE_ASPECT","fill",1);
 int screens[][3]={{2712,1220,1080},{2340,1080,1056},{1920,1080,864},{1280,800,776},{640,480,648},{0,0,864},{10000,1000,1296}};
 for(unsigned i=0;i<sizeof(screens)/sizeof(screens[0]);i++) {
  width=640;height=480;DRAndroid_ConfigureAspect(&width,&height,screens[i][0],screens[i][1]);
  assert(width==screens[i][2]&&height==486&&width%8==0);
  assert(gGraf_specs[1].row_bytes==width&&gGraf_data[1].width==width);
  DRAndroid_ConfigureAspect(&width,&height,screens[i][1],screens[i][0]);assert(width==screens[i][2]);
 }
 width=320;height=200;DRAndroid_ConfigureAspect(&width,&height,2712,1220);assert(width==320&&height==200);
 // Original cockpit pixels overlay the world; all outside pixels must survive.
 _Alignas(2) tS8 art[]={4,0,1,-4,10,20,30,40,1,4,3,1,-2,70,80,1,3,-1,33,2,-1,44};
 int pixels[4][4]={{10,20,30,40},{-1,-1,-1,-1},{-1,70,80,-1},{33,-1,-1,44}};
 unsigned char storage[16+16*6+16];br_pixelmap pm={12,6,16,storage+16};
 for(int wobble_x=-2;wobble_x<=2;wobble_x+=2)for(int wobble_y=-2;wobble_y<=2;wobble_y+=2) {
  memset(storage,77,sizeof(storage));
  CopyStripImage(&pm,4,wobble_x,1,wobble_y,art,0,0,4,4);
  for(int y=0;y<6;y++)for(int x=0;x<16;x++) {
   int sx=x-4-wobble_x,sy=y-1-wobble_y,expected=77;
   if(sx>=0&&sx<4&&sy>=0&&sy<4&&pixels[sy][sx]>=0)expected=pixels[sy][sx];
   assert(storage[16+y*16+x]==expected);
  }
  for(int i=0;i<16;i++)assert(storage[i]==77&&storage[112+i]==77);
 }
 puts("PASS aspect modes, limits, native cockpit pixels, transparent windows, visible world at both sides, wobble/clipping and guard bytes");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run(['cc','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
# Use actual production flush in a real GLES context with shared-parent sub-pixmaps.
base=runpy.run_path(str(root/'test/android/test_gpu_batch.py'))
extra=r'''
static void offset_exercise(void) {
 for(int factor=1;factor<=3;factor++) {
  prepare_scene(factor);
  br_pixelmap sub={cpu_pixels,11,7,33,3,4,NULL};
  last_colour_buffer=&sub;last_depth_buffer=&depth;
  rect(3,25-4-7,11,7,87,0);GLRenderer_FlushBuffers();
  for(int y=0;y<25;y++)for(int x=0;x<33;x++)
   assert(cpu_pixels[y*33+x]==(y>=4&&y<11&&x>=3&&x<14?87:5));
  assert(!memcmp(snapshot_data,cpu_pixels,sizeof(cpu_pixels)));
  last_colour_buffer=&colour;
 }
 puts("PASS production offset readback lands in the shared parent at base_x/base_y, preserving borders at 1x/2x/3x");
}
'''
source=base['source'];i=source.index('int main(void)');source=source[:i]+extra+'\n'+source[i:];source=source.replace('blend_id=texture(256,256,table);exercise();','blend_id=texture(256,256,table);exercise();offset_exercise();')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run(['cc','-std=c99','-I'+str(root/'lib/glad/include'),str(p/'test.c'),str(root/'lib/glad/src/glad.c'),'-l:libEGL.so.1','-ldl','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'LIBGL_ALWAYS_SOFTWARE':'1'})
