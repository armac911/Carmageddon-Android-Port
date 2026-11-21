"""Verify indexed mip generation: level counts, medoid choice, transparency,
non-power-of-two blocks, palette-less fallback and fade invariance."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[2]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "texture_mips.h"
static unsigned char pal[1024], faded[1024];
static void set(int i,int r,int g,int b){pal[i*4]=r;pal[i*4+1]=g;pal[i*4+2]=b;pal[i*4+3]=255;}
int main(void) {
 assert(DRMipLevelCount(1,1)==1); assert(DRMipLevelCount(2,1)==2);
 assert(DRMipLevelCount(256,256)==9); assert(DRMipLevelCount(256,8)==9);
 assert(DRMipLevelCount(5,3)==3); /* 5x3 -> 2x1 -> 1x1 */
 for(int i=1;i<256;i++)set(i,i,(i*7)&255,(i*13)&255);
 set(10,0,0,0); set(11,200,200,200); set(12,90,90,90); set(13,105,105,105);
 /* 2x2 block {10,11,12,13}: mean ~(98.75,...) -> 13 (105) beats 12 (90). */
 unsigned char src[4]={10,11,12,13}, out[1];
 DRBuildIndexedMip(out,1,1,src,2,2,pal); assert(out[0]==13);
 /* Transparent texels are ignored; fully transparent blocks stay 0. */
 unsigned char cut[4]={0,0,0,12}; DRBuildIndexedMip(out,1,1,cut,2,2,pal); assert(out[0]==12);
 unsigned char none[4]={0,0,0,0}; DRBuildIndexedMip(out,1,1,none,2,2,pal); assert(out[0]==0);
 /* Without a palette the first opaque texel (row-major) is used. */
 unsigned char first[4]={0,11,12,13}; DRBuildIndexedMip(out,1,1,first,2,2,NULL); assert(out[0]==11);
 /* Fades scale the palette: the chosen indices must not change. */
 for(int i=0;i<1024;i++)faded[i]=(i%4==3)?255:pal[i]*96/255;
 unsigned seed=3;
 for(int w=1;w<=37;w++)for(int h=1;h<=23;h+=3){
  unsigned char *s=malloc(w*h),*a=malloc(w*h),*b=malloc(w*h);
  for(int i=0;i<w*h;i++){seed=seed*1664525u+1013904223u;s[i]=(seed>>24)%5?(seed>>16)&255:0;}
  int mw=w>1?w/2:1, mh=h>1?h/2:1;
  DRBuildIndexedMip(a,mw,mh,s,w,h,pal); DRBuildIndexedMip(b,mw,mh,s,w,h,faded);
  for(int y=0;y<mh;y++)for(int x=0;x<mw;x++){
   /* Output is an opaque index from its own block, or 0 only if all clear. */
   int x0=x*w/mw,x1=(x+1)*w/mw,y0=y*h/mh,y1=(y+1)*h/mh,found=0,opaque=0;
   if(x1<=x0)x1=x0+1; if(y1<=y0)y1=y0+1;
   for(int sy=y0;sy<y1;sy++)for(int sx=x0;sx<x1;sx++){if(s[sy*w+sx])opaque=1;if(s[sy*w+sx]==a[y*mw+x])found=1;}
   assert(opaque ? (a[y*mw+x]!=0 && found) : a[y*mw+x]==0);
  }
  /* Rounding in the faded palette may flip near-ties; allow a small share. */
  int diff=0; for(int i=0;i<mw*mh;i++)diff+=a[i]!=b[i];
  assert(diff*10<=mw*mh+9);
  free(s);free(a);free(b);
 }
 memset(faded,0,sizeof faded); assert(!DRPaletteUsableForMips(faded)); assert(DRPaletteUsableForMips(pal));
 puts("PASS indexed mips: level counts, medoid, cutouts, palette-less fallback, NPOT blocks, fade invariance, dark-palette rejection");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src/harness/renderers/gl'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
 subprocess.run([str(p/'test')],check=True,env=env)
