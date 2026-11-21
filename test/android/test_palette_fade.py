"""Palette fades reuse the filter LUT: DRPaletteFadeScale must accept every
step of the game's own fade (Darken: v*degree/256) and reject real changes."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[2]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "palette_filter.h"
static unsigned char base[1024], now[1024];
int main(void) {
 unsigned seed=11;
 for(int i=0;i<1024;i++){seed=seed*1664525u+1013904223u;base[i]=(i%4==3)?255:(seed>>24);}
 /* Every degree SetFadedPalette uses, down to black. */
 for(int degree=256;degree>=0;degree--){
  for(int i=0;i<1024;i++)now[i]=(i%4==3)?base[i]:(unsigned char)((degree*base[i])/256);
  float k=DRPaletteFadeScale(base,now);
  assert(k>=0.f);
  assert(k>degree/256.f-0.02f && k<degree/256.f+0.02f);
 }
 /* Fading back up from a darker LUT palette is accepted up to 2x. */
 for(int i=0;i<1024;i++)now[i]=(i%4==3)?base[i]:(unsigned char)((160*base[i])/256);
 assert(DRPaletteFadeScale(now,base)>1.5f);
 for(int i=0;i<1024;i++)now[i]=(i%4==3)?base[i]:(unsigned char)((64*base[i])/256);
 assert(DRPaletteFadeScale(now,base)<0.f);
 /* A different palette (one entry swapped) forces a rebuild. */
 memcpy(now,base,sizeof now); now[40]^=0x80; assert(DRPaletteFadeScale(base,now)<0.f);
 /* Hue shifts (e.g. drug palette) are not fades. */
 for(int i=0;i<1024;i++)now[i]=(i%4==3)?base[i]:base[(i/4)*4+((i%4)+1)%3];
 assert(DRPaletteFadeScale(base,now)<0.f);
 /* An all-black LUT palette only matches all-black. */
 unsigned char black[1024]={0}; assert(DRPaletteFadeScale(black,black)==1.f); assert(DRPaletteFadeScale(black,base)<0.f);
 puts("PASS palette fade detection: all 257 fade steps reuse the LUT; brightening, swaps, hue shifts and black handled");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src/harness/renderers/gl'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
 subprocess.run([str(p/'test')],check=True,env=env)
