#include "../../src/harness/platforms/frame_pacer.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 for(unsigned cap=60;cap<=144;cap+=cap==60?60:24) {
  DRFramePacer p={0};uint64_t now=1000000000ull, frequency=1000000000ull;
  assert(!DRFrameWait(&p,now,frequency,cap));DRFrameCommit(&p,now);
  for(int i=0;i<10000;i++) {
   uint64_t before=p.last;now+= i%17==0?30000000:1000000;
   now+=DRFrameWait(&p,now,frequency,cap);
   assert(now-before>=(frequency+cap-1)/cap);DRFrameCommit(&p,now);
  }
  now+=5000000000ull;assert(!DRFrameWait(&p,now,frequency,cap));DRFrameCommit(&p,now);
  assert(DRFrameWait(&p,now+1000000,frequency,cap)>0);
  assert(!DRFrameWait(&p,now+1000000,frequency,0));
 }
 puts("PASS 60/120/144 caps, short frames, slow frames, pause, uncapped");
}
