/* Deterministic test of production aggregation, interruption, limits and reset. */
#define __ANDROID__ 1
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
static uint64_t fake_ns=1000000000ull;
static int fake_clock(clockid_t id,struct timespec *t) {t->tv_sec=fake_ns/1000000000ull;t->tv_nsec=fake_ns%1000000000ull;return 0;}
#define clock_gettime fake_clock
#include "../../src/harness/platforms/profiler.c"
int main(void) {
    char path[]="/tmp/carma-profile-test-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    setenv("DETHRACE_PROFILE_PATH",path,1);DRProfileGPU("vendor","renderer","version");DRProfileRace();
    DRProfileBegin();uint64_t t=DRProfileClock();fake_ns+=10000000;DRProfileTime(DRP_UPDATE,t);
    DRProfileCount(DRP_DRAWS,4);DRProfileCount(DRP_TRIANGLES,20);fake_ns+=20000000;DRProfileEnd();
    assert(times[DRP_UPDATE]==10000000 && counts[DRP_DRAWS]==4 && total_ns==30000000);
    DRProfileBegin();DRProfileMarkPause();fake_ns+=5000000000ull;DRProfileEnd();assert(total_ns==30000000);
    DRProfileBegin();assert(counts[DRP_DRAWS]==0);fake_ns+=119970000000ull;DRProfileEnd();assert(total_ns==120000000000ull);
    DRProfileBegin();assert(!active);finish();
    FILE*f=fopen(path,"r");assert(f);char line[2048];unsigned rows=0;while(fgets(line,sizeof(line),f))if(line[0]>='0'&&line[0]<='9'){++rows;unsigned commas=0;for(char*p=line;*p;p++)commas+=*p==',';assert(commas==20);}fclose(f);assert(rows==3);
    setenv("DETHRACE_PROFILE_PATH","",1);DRProfileGPU("","","");DRProfileBegin();assert(!active&&!output&&total_ns==0);
    setenv("DETHRACE_PROFILE_PATH",path,1);DRProfileGPU("","","");DRProfileRace();DRProfileBegin();assert(active&&race==1);fake_ns+=10000000;DRProfileEnd();finish();unlink(path);
    puts("Profiler aggregation, pause exclusion, capture cap, CSV schema, disabled mode and session reset passed");
}
