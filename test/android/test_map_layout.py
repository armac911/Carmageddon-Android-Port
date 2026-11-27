"""Production wide-map composition: proportions, placement, blips and CPU ownership."""
from pathlib import Path
import subprocess,tempfile,os,runpy
root=Path(__file__).resolve().parents[2]
s=(root/'src/DETHRACE/common/graphics.c').read_text()
a=s.index('static int DRAndroidComposeMap(');b=s.index('\n#endif',a);fn=s[a:b]
s=(root/'src/harness/renderers/gl/gl_renderer.c').read_text();a=s.index('    for(int y=cpu_rect[1]');b=s.index('\n#endif',a);force=s[a:b]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t tU8;
typedef struct {void* pixels;int width,height,row_bytes;} br_pixelmap;
static int cpu_rect[4],calls;
static void DRPresentCpuRect(int x,int y,int w,int h){cpu_rect[0]=x;cpu_rect[1]=y;cpu_rect[2]=w;cpu_rect[3]=h;calls++;}
'''+fn+r'''
static unsigned char original(int x,int y){return (x*5+y*3)%253+1;}
int main(void){
 int widths[]={640,648,864,1056,1080,1296};
 for(unsigned n=0;n<sizeof(widths)/sizeof(widths[0]);n++){
  int w=widths[n],h=486,stride=w+7,render_width=w,render_height=h;
  unsigned char* raw=malloc(stride*h+32);memset(raw,199,stride*h+32);
  unsigned char* pixels=raw+16;
  for(int y=0;y<480;y++)for(int x=0;x<640;x++)pixels[y*stride+x]=original(x,y);
  // A checkpoint and a live-preview pixel are part of the same composition.
  pixels[120*stride+320]=254;pixels[50*stride+30]=255;
  br_pixelmap target={pixels,w,h,stride};calls=0;assert(DRAndroidComposeMap(&target)==1&&calls==1);
  assert(cpu_rect[0]==0&&cpu_rect[1]==0&&cpu_rect[2]==w&&cpu_rect[3]==h);
  int draw_w=w,draw_h=w*480/640;
  if(draw_h>h){draw_h=h;draw_w=h*640/480;}
  int left=(w-draw_w)/2,top=(h-draw_h)/2,checkpoint=0,preview=0;
  for(int y=0;y<h;y++)for(int x=0;x<stride;x++){
   int expected=x>=w?199:0;
   if(x>=left&&x<left+draw_w&&y>=top&&y<top+draw_h){int sx=(x-left)*640/draw_w,sy=(y-top)*480/draw_h;
    expected=sy==120&&sx==320?254:sy==50&&sx==30?255:original(sx,sy);
    checkpoint+=expected==254;preview+=expected==255;
   }
   assert(pixels[y*stride+x]==expected);
  }
  assert(checkpoint&&preview);
  for(int i=0;i<16;i++)assert(raw[i]==199&&raw[16+stride*h+i]==199);
  // Run the real presenter's CPU-rectangle loop, including palette index zero.
  unsigned char* screen_buffer=malloc(w*h),*snapshot_pixels=calloc(w*h,1);
  for(int y=0;y<h;y++)memcpy(screen_buffer+y*w,pixels+y*stride,w);
'''+force+r'''
  for(int i=0;i<w*h;i++)assert(snapshot_pixels[i]!=screen_buffer[i]);
  free(screen_buffer);free(snapshot_pixels);free(raw);
 }
 unsigned char classic[640*480];memset(classic,71,sizeof(classic));
 br_pixelmap pm={classic,640,480,640};calls=0;assert(!DRAndroidComposeMap(&pm)&&!calls);
 for(unsigned i=0;i<sizeof(classic);i++)assert(classic[i]==71);
 pm.width=320;pm.height=200;pm.row_bytes=320;assert(!DRAndroidComposeMap(&pm));
 assert(!DRAndroidComposeMap(NULL));pm.width=1080;pm.height=486;pm.row_bytes=1079;assert(!DRAndroidComposeMap(&pm));
 puts("PASS map aspect/centring, opaque side bars, markers/live preview, row padding, bounds, legacy paths and production CPU-present ownership");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
# Actual palette compositor: a nonzero GPU race image must not leak through black bars.
g=runpy.run_path(str(root/'test/android/presentation_filter_test.py'))
image=[0 if x<2 or x>=6 else 1+(x+y)%4 for y in range(8) for x in range(8)]
g['replace'](0,8,8,image);g['replace'](2,8,8,[v^255 for v in image]);g['replace'](3,8,8,[4]*64)
result=g['render'](0)
for i,value in enumerate(image):assert result[4*i:4*i+4]==list(g['rgb'][value])
print('PASS actual GLES map/black-side-bar presentation over stale nonzero race geometry')
