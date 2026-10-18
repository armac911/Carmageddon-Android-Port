#ifndef DR_PROFILER_H
#define DR_PROFILER_H
#include <stdint.h>
enum { DRP_UPDATE, DRP_RENDER, DRP_READBACK, DRP_COPY, DRP_PRESENT, DRP_SWAP, DRP_MODEL, DRP_LIMITER, DRP_PHASES };
enum { DRP_MODELS, DRP_CULLED, DRP_DRAWS, DRP_TRIANGLES, DRP_MATERIALS, DRP_UPLOADS, DRP_READS, DRP_BYTES, DRP_COUNTS };
#ifdef __ANDROID__
void DRProfileRace(void);
void DRProfileBegin(void);
void DRProfileEnd(void);
void DRProfileMarkPause(void);
uint64_t DRProfileClock(void);
void DRProfileTime(int phase, uint64_t start);
void DRProfileCount(int counter, uint64_t value);
void DRProfileGPU(const char *vendor, const char *renderer, const char *version);
#else
#define DRProfileRace() ((void)0)
#define DRProfileBegin() ((void)0)
#define DRProfileEnd() ((void)0)
#define DRProfileMarkPause() ((void)0)
#define DRProfileClock() ((uint64_t)0)
#define DRProfileTime(a,b) ((void)0)
#define DRProfileCount(a,b) ((void)0)
#define DRProfileGPU(a,b,c) ((void)0)
#endif
#endif
