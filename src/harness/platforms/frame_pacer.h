#ifndef DR_FRAME_PACER_H
#define DR_FRAME_PACER_H
#include <stdint.h>
typedef struct { uint64_t last; int started; } DRFramePacer;
static inline uint64_t DRFrameWait(DRFramePacer *p,uint64_t now,uint64_t frequency,unsigned fps) {
    if (!fps || !frequency || !p->started || now < p->last) return 0;
    uint64_t period=(frequency+fps-1)/fps;
    uint64_t elapsed=now-p->last;
    return elapsed<period?period-elapsed:0;
}
static inline void DRFrameCommit(DRFramePacer *p,uint64_t now) {p->last=now;p->started=1;}
#endif
