#include "harness/profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
static FILE *output;
static int initialized, active, race, epoch;
static unsigned frames;
static atomic_int pause_epoch;
static uint64_t start_ns, session_ns, flush_ns, total_ns;
static uint64_t times[DRP_PHASES], counts[DRP_COUNTS];
static char gpu_vendor[160], gpu_renderer[160], gpu_version[160];
static uint64_t now_ns(void) {
    struct timespec t; clock_gettime(CLOCK_BOOTTIME, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + t.tv_nsec;
}
static void finish(void);
void DRProfileGPU(const char *v,const char *r,const char *s) {
    finish(); initialized=active=race=0; frames=0; total_ns=0;
    snprintf(gpu_vendor,sizeof(gpu_vendor),"%s",v?v:"");
    snprintf(gpu_renderer,sizeof(gpu_renderer),"%s",r?r:"");
    snprintf(gpu_version,sizeof(gpu_version),"%s",s?s:"");
}
static void finish(void) { if(output) { fflush(output); fclose(output); output=NULL; } }
void DRProfileRace(void) { ++race; }
void DRProfileMarkPause(void) { atomic_fetch_add(&pause_epoch,1); }
void DRProfileBegin(void) {
    active=0;
    if(!initialized) {
        initialized=1;
        const char *path=getenv("DETHRACE_PROFILE_PATH");
        if(path && *path) output=fopen(path,"w");
        if(output) {
            setvbuf(output,NULL,_IOFBF,65536);
            fprintf(output,"# CPU wall timings; render includes readback/copy/present/swap. Do not sum nested fields.\n");
            fprintf(output,"# GPU vendor: %s; renderer: %s; version: %s\n",gpu_vendor,gpu_renderer,gpu_version);
            fprintf(output,"# Capture limit: 120 seconds of completed race frames; excluded=1 means pause/menu interrupted frame.\n");
            fprintf(output,"session_ms,race,frame,excluded,frame_ms,update_ms,render_ms,readback_ms,copy_ms,present_ms,swap_ms,model_submit_ms,limiter_ms,models,culled,draw_calls,triangles,material_changes,model_uploads,readbacks,readback_bytes\n");
            session_ns=flush_ns=now_ns();
            fprintf(output,"# session_start_elapsed_ms=%.3f\n",session_ns/1e6);
            fflush(output);
            static int cleanup_registered;
            if(!cleanup_registered) { atexit(finish); cleanup_registered=1; }
        }
    }
    if(!output || total_ns>=120000000000ull) return;
    memset(times,0,sizeof(times)); memset(counts,0,sizeof(counts));
    epoch=atomic_load(&pause_epoch); start_ns=now_ns(); active=1;
}
uint64_t DRProfileClock(void) { return active?now_ns():0; }
void DRProfileTime(int phase,uint64_t start) { if(active && start) times[phase]+=now_ns()-start; }
void DRProfileCount(int counter,uint64_t value) { if(active) counts[counter]+=value; }
void DRProfileEnd(void) {
    if(!active) return;
    uint64_t end=now_ns(), duration=end-start_ns;
    int excluded=epoch!=atomic_load(&pause_epoch);
    if(!excluded) total_ns+=duration;
    fprintf(output,"%.3f,%d,%u,%d,%.3f",(end-session_ns)/1e6,race,++frames,excluded,duration/1e6);
    for(int i=0;i<DRP_PHASES;i++) fprintf(output,",%.3f",times[i]/1e6);
    for(int i=0;i<DRP_COUNTS;i++) fprintf(output,",%llu",(unsigned long long)counts[i]);
    fputc('\n',output); active=0;
    if(end-flush_ns>=1000000000ull || total_ns>=120000000000ull || excluded) { fflush(output); flush_ns=end; }
}
