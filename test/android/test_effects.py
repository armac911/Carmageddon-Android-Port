"""Enhanced effects: with the options off every count and pool limit equals
the original game's; with them on they grow but stay inside the 32-bit flag
masks and never produce fewer particles than the original."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[2]
code=r'''
#define __ANDROID__ 1
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "android_effects.h"
int main(int argc,char**argv){
 int on=argc>1&&argv[1][0]=='1';
 assert(DRAndroidTyreEffects()==on && DRAndroidDebrisEffects()==on);
 assert(DR_SMOKE_POOL<=32 && DR_SHRAPNEL_POOL<=32 && DR_SKID_POOL<=65535); /* flag masks / tU16 replay index */
 if(!on){
  assert(DRSkidLimit()==100 && DRSmokeLimit()==25 && DRShrapnelLimit()==15);
  for(int n=0;n<40;n++){ assert(DRSparkCount(n,10)==(n>10?10:n)); assert(DRShrapnelCount(n)==n); }
 } else {
  assert(DRSkidLimit()==300 && DRSmokeLimit()==32 && DRShrapnelLimit()==32);
  for(int n=0;n<40;n++){
   int capped=n>10?10:n;
   assert(DRSparkCount(n,10)>=capped);                 /* never fewer than the original */
   assert(DRSparkCount(n,10)<=(n>16?n:16));             /* bounded */
   assert(DRShrapnelCount(n)>=(n<32?n:32) && DRShrapnelCount(n)<=32);
  }
  assert(DRSparkCount(30,30)==30);                       /* big showers are not reduced */
 }
 /* Tyre smoke strength: bounded, monotonic, stronger than the original pV/25. */
 float last=0;
 for(float v=0;v<40;v+=0.5f){float s=DRTyreSmokeStrength(v);assert(s>=0.25f&&s<=0.9f&&s>=last);last=s;if(v<22)assert(s>=v/25.f);}
 printf("PASS effects %s: pool limits, spark/shrapnel counts, tyre smoke strength\n",on?"enhanced":"original");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(root/'src/DETHRACE'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 for on in ('0','1'):
  env=os.environ.copy();env['DETHRACE_FX_TYRES']=env['DETHRACE_FX_DEBRIS']='on' if on=='1' else 'off'
  subprocess.run([str(p/'test'),on],check=True,env=env)
