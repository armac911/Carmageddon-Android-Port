/* Low-overhead race-only aggregate wall timings. Nested phases must not be added. */
#include "harness/profiler.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static uint64_t frame_start, report_start, total;
static uint64_t frame_times[DRP_PHASES], times[DRP_PHASES];
static uint64_t frame_counts[DRP_COUNTS], counts[DRP_COUNTS];
static unsigned frames;
static int active, excluded;
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
void DRProfileGPU(const char* a,const char* b,const char* c){(void)a;(void)b;(void)c;}
void DRProfileRace(void){
    memset(times,0,sizeof(times));memset(counts,0,sizeof(counts));frames=0;total=0;active=0;report_start=now();
    puts("MIYOO_PROFILE race start; render includes model,copy,present,swap,limiter; model includes geometry+raster; do not sum nested timings");
}
void DRProfileMarkPause(void){excluded=1;}
void DRProfileBegin(void){
    memset(frame_times,0,sizeof(frame_times));memset(frame_counts,0,sizeof(frame_counts));
    excluded=0;active=1;frame_start=now();
}
uint64_t DRProfileClock(void){return active?now():0;}
void DRProfileTime(int phase,uint64_t start){if(active&&start&&phase>=0&&phase<DRP_PHASES)frame_times[phase]+=now()-start;}
void DRProfileCount(int counter,uint64_t value){if(active&&counter>=0&&counter<DRP_COUNTS)frame_counts[counter]+=value;}
void DRProfileEnd(void){
    if(!active)return;
    uint64_t end=now();active=0;
    if(excluded)return;
    total+=end-frame_start;frames++;
    for(int i=0;i<DRP_PHASES;i++)times[i]+=frame_times[i];
    for(int i=0;i<DRP_COUNTS;i++)counts[i]+=frame_counts[i];
    if(end-report_start<5000000000ull)return;
    double divisor=frames*1000000.0;
    printf("MIYOO_PROFILE frames=%u frame_ms=%.3f update_ms=%.3f render_ms=%.3f model_ms=%.3f geometry_ms=%.3f raster_ms=%.3f palette_ms=%.3f present_ms=%.3f swap_ms=%.3f limiter_ms=%.3f models_per_frame=%.1f triangles_per_frame=%.1f vertices_per_frame=%.1f clipped_per_frame=%.1f rejected_per_frame=%.1f\n",
        frames,total/divisor,times[DRP_UPDATE]/divisor,times[DRP_RENDER]/divisor,times[DRP_MODEL]/divisor,
        times[DRP_GEOMETRY]/divisor,times[DRP_RASTER]/divisor,times[DRP_COPY]/divisor,times[DRP_PRESENT]/divisor,
        times[DRP_SWAP]/divisor,times[DRP_LIMITER]/divisor,(double)counts[DRP_MODELS]/frames,
        (double)counts[DRP_TRIANGLES]/frames,(double)counts[DRP_VERTICES]/frames,
        (double)counts[DRP_CLIPPED]/frames,(double)counts[DRP_REJECTED]/frames);
    memset(times,0,sizeof(times));memset(counts,0,sizeof(counts));total=0;frames=0;report_start=end;
}
