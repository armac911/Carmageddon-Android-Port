"""Verify transparent row merge bounds, tails, offsets and all palette values."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[2]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "indexed_copy.h"
int main(void) {
 unsigned seed=7;
 for(int width=0;width<=1057; width++) for(int off=0;off<17;off++) {
  int n=width+off+19;
  unsigned char *src=malloc(n),*dst=malloc(n),*expected=malloc(n);
  assert(src&&dst&&expected);
  for(int i=0;i<n;i++) {seed=seed*1664525u+1013904223u;src[i]=(seed>>24);dst[i]=(unsigned char)i;}
  memcpy(expected,dst,n);
  for(int x=0;x<width;x++){if((x%3)==0)src[off+x]=0;if(src[off+x])expected[off+x]=src[off+x];}
  DRMergeIndexedRow(dst+off,src+off,width);assert(!memcmp(dst,expected,n));
  memset(src,0,n);memcpy(expected,dst,n);DRMergeIndexedRow(dst+off,src+off,width);assert(!memcmp(dst,expected,n));
  memset(src,255,n);for(int x=0;x<width;x++)expected[off+x]=255;
  DRMergeIndexedRow(dst+off,src+off,width);assert(!memcmp(dst,expected,n));
  free(src);free(dst);free(expected);
 }
 puts("PASS indexed row merge: mixed/zero/opaque pixels, all widths 0..1057, offsets, vector tails and untouched padding");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-O2','-fsanitize=address,undefined','-I'+str(root/'src/harness/renderers/gl'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
 subprocess.run([str(p/'test')],check=True,env=env)
