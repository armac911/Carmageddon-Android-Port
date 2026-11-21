"""Track column wedge culling must be conservative: no point inside the real
view volume may ever be rejected, for any yaw/pitch/roll, FOV and aspect.
It must also actually reject columns behind and well beside the camera."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[2]
code=r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "column_cull.h"
static double rnd(void){return rand()/(double)RAND_MAX;}
/* Camera-to-world rows from yaw/pitch/roll (camera looks down -Z). */
static void camera(float m[4][3], double yaw, double pitch, double roll, double px, double pz) {
 double cy=cos(yaw),sy=sin(yaw),cp=cos(pitch),sp=sin(pitch),cr=cos(roll),sr=sin(roll);
 /* R = Ry(yaw) * Rx(pitch) * Rz(roll); rows are the camera axes in world space. */
 double R[3][3]={{cy*cr+sy*sp*sr, cp*sr, -sy*cr+cy*sp*sr},
                 {-cy*sr+sy*sp*cr, cp*cr, sy*sr+cy*sp*cr},
                 {sy*cp, -sp, cy*cp}};
 for(int i=0;i<3;i++)for(int j=0;j<3;j++)m[i][j]=(float)R[i][j];
 m[3][0]=(float)px;m[3][1]=2.f;m[3][2]=(float)pz;
}
int main(void){
 srand(7);
 long checked=0,rejected_ok=0,enabled_cases=0;
 for(int c=0;c<20000;c++){
  double vfov=(50+rnd()*50)*M_PI/180, aspect=1.0+rnd()*1.3;
  double yaw=rnd()*2*M_PI, pitch=(rnd()-0.5)*1.6, roll=(rnd()-0.5)*0.9;
  float m[4][3];camera(m,yaw,pitch,roll,(rnd()-0.5)*100,(rnd()-0.5)*100);
  tDR_column_wedge w;DRColumnWedgeSetup(&w,(const float(*)[3])m,(float)vfov,(float)aspect,8.f);
  if(w.enabled)enabled_cases++;
  for(int k=0;k<200;k++){
   /* Random point inside the frustum: NDC x,y in [-1,1], depth 0.1..400. */
   double d=0.1+rnd()*400, x=(rnd()*2-1)*tan(vfov/2)*aspect*d, y=(rnd()*2-1)*tan(vfov/2)*d;
   double wx=m[3][0]+x*m[0][0]+y*m[1][0]-d*m[2][0];
   double wz=m[3][2]+x*m[0][2]+y*m[1][2]-d*m[2][2];
   assert(DRColumnInWedge(&w,(float)wx-0.01f,(float)wz-0.01f,(float)wx+0.01f,(float)wz+0.01f));
   checked++;
  }
 }
 assert(enabled_cases>10000); /* it must be active for typical driving cameras */
 /* Level camera at the origin looking down -Z with a wide phone aspect. */
 float m[4][3];camera(m,0,0,0,0,0);
 tDR_column_wedge w;DRColumnWedgeSetup(&w,(const float(*)[3])m,(float)(70*M_PI/180),2.2f,8.f);
 assert(w.enabled);
 assert(DRColumnInWedge(&w,-4,-60,4,-52));      /* straight ahead */
 assert(DRColumnInWedge(&w,-4,-4,4,4));         /* the camera's own cell */
 assert(!DRColumnInWedge(&w,-4,40,4,48));       /* well behind */
 assert(!DRColumnInWedge(&w,200,-10,208,-2));   /* far to the side, near the camera line */
 rejected_ok=1;
 /* Disabled for steep pitch (map-style views) and degenerate inputs. */
 camera(m,0,-1.3,0,0,0);DRColumnWedgeSetup(&w,(const float(*)[3])m,1.2f,1.5f,8.f);assert(!w.enabled);
 camera(m,0,0,0,0,0);DRColumnWedgeSetup(&w,(const float(*)[3])m,1.2f,1.5f,0.f);assert(!w.enabled);
 DRColumnWedgeSetup(&w,(const float(*)[3])m,3.0f,3.0f,8.f);assert(!w.enabled); /* ~180 degree view */
 printf("PASS column wedge culling: %ld random in-view points never rejected over 20000 cameras (yaw/pitch/roll, FOV 50-100, aspect 1-2.3); rejects behind/side; disables itself when unsafe%s\n",checked,rejected_ok?"":"");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src/DETHRACE'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
 subprocess.run([str(p/'test')],check=True,env=env)
