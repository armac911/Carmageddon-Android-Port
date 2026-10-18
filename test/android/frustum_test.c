#include "../../src/harness/renderers/gl/frustum.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint32_t seed=1234567;
static float randomf(void) {seed=1664525*seed+1013904223;return ((seed>>8)/(float)0xffffff)*2-1;}
static void identity(float m[4][4]) {memset(m,0,16*sizeof(float));for(int i=0;i<4;i++)m[i][i]=1;}
/* Independent oracle: transform all eight corners using the shader's exact
 * model -> view -> projection order, then test the six homogeneous planes. */
static int reference(const float lo[3],const float hi[3],const float m[4][3],const float v[4][4],const float p[4][4]) {
 int outside[6]={0};
 for(int c=0;c<8;c++) {
  double a[4],world[4]={0,0,0,1},eye[4]={0},clip[4]={0};
  for(int i=0;i<3;i++)a[i]=(c&(1<<i))?hi[i]:lo[i];
  a[3]=1;
  for(int i=0;i<3;i++)for(int j=0;j<4;j++)world[i]+=a[j]*m[j][i];
  for(int i=0;i<4;i++)for(int j=0;j<4;j++)eye[i]+=world[j]*v[j][i];
  for(int i=0;i<4;i++)for(int j=0;j<4;j++)clip[i]+=eye[j]*p[j][i];
  for(int i=0;i<3;i++){outside[i*2]+=clip[3]+clip[i]<0;outside[i*2+1]+=clip[3]-clip[i]<0;}
 }
 for(int i=0;i<6;i++)if(outside[i]==8)return 0;
 return 1;
}
int main(void) {
 float v[4][4],p[4][4],planes[6][4],m[4][3]={{1,0,0},{0,1,0},{0,0,1},{0,0,0}};
 float lo[3]={-.2f,-.2f,-.2f},hi[3]={.2f,.2f,.2f};identity(v);identity(p);DRFrustumPlanes(planes,v,p);
 assert(DRBoundsVisible(lo,hi,m,planes));
 for(int axis=0;axis<3;axis++)for(int sign=-1;sign<=1;sign+=2){m[3][axis]=sign*2.f;assert(!DRBoundsVisible(lo,hi,m,planes));m[3][axis]=sign*1.2f;assert(DRBoundsVisible(lo,hi,m,planes));m[3][axis]=0;}
 float largeLo[3]={-3,-3,-3},largeHi[3]={3,3,3};assert(DRBoundsVisible(largeLo,largeHi,m,planes));
 // 4:3 / 16:9, normal / doubled far planes; camera rotations, translations,
 // reflected/scaled/sheared models and boxes straddling the near plane.
 int rejected=0,extra=0;
 for(int aspect=0;aspect<2;aspect++)for(int far=35;far<=70;far+=35) {
  memset(p,0,sizeof(p));p[0][0]=1.5f/(aspect?16.f/9:4.f/3);p[1][1]=1.5f;p[2][2]=-(far+.1f)/(far-.1f);p[2][3]=-1;p[3][2]=-(2*far*.1f)/(far-.1f);
  for(int trial=0;trial<10000;trial++) {
   identity(v);float angle=randomf()*3.14f;v[0][0]=v[2][2]=cosf(angle);v[0][2]=sinf(angle);v[2][0]=-sinf(angle);
   for(int a=0;a<3;a++){v[3][a]=randomf()*10;for(int b=0;b<3;b++)m[a][b]=randomf()*3;m[3][a]=randomf()*100;lo[a]=-fabsf(randomf()*5);hi[a]=fabsf(randomf()*5);}
   DRFrustumPlanes(planes,v,p);int expected=reference(lo,hi,m,v,p),actual=DRBoundsVisible(lo,hi,m,planes);
   assert(!expected||actual);rejected+=!actual;extra+=actual&&!expected;
  }
 }
 assert(rejected>20000); // Ensure the test exercises actual rejection, not fail-open only.
 identity(v);identity(p);DRFrustumPlanes(planes,v,p);m[3][0]=NAN;assert(DRBoundsVisible(lo,hi,m,planes));
 printf("PASS: 40,000 transformed boxes; no visible rejection; %d rejected, %d conservative extras\n",rejected,extra);
}
