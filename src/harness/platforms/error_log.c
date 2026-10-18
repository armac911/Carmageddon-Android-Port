#define _GNU_SOURCE
#include "error_log.h"
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>

#define LOG_LIMIT (2u * 1024u * 1024u)
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static int log_fd = -1;
static volatile sig_atomic_t crash_fd = -1;
static size_t written;
static char log_path[4096], previous_path[4112];
static const int signals[] = {SIGSEGV, SIGFPE, SIGBUS, SIGILL, SIGABRT};
static struct sigaction previous[5];
static int installed[5];

/* No formatting/allocation/locks in the signal handler. Android's existing
 * handler receives the original signal context, so debuggerd can still run. */
static char* append_hex(char* p, uintptr_t value) {
    static const char digits[] = "0123456789abcdef";
    char reversed[2 * sizeof(uintptr_t)]; int n = 0;
    do { reversed[n++] = digits[value & 15u]; value >>= 4; } while (value);
    *p++ = '0'; *p++ = 'x';
    while (n) *p++ = reversed[--n];
    return p;
}
static void record_signal(int sig, siginfo_t* info, void* context) {
    char message[160]; char* p = message;
    const char* text = "signal="; while (*text) *p++ = *text++;
    p = append_hex(p, (uintptr_t)sig);
    text = " code="; while (*text) *p++ = *text++;
    p = append_hex(p, info ? (uintptr_t)info->si_code : 0);
    text = " fault="; while (*text) *p++ = *text++;
    p = append_hex(p, info && (sig==SIGSEGV || sig==SIGBUS) ? (uintptr_t)info->si_addr : 0);
    uintptr_t pc = 0;
#if defined(__aarch64__)
    if (context) pc = ((ucontext_t*)context)->uc_mcontext.pc;
#elif defined(__x86_64__) && defined(REG_RIP)
    if (context) pc = ((ucontext_t*)context)->uc_mcontext.gregs[REG_RIP];
#endif
    text = " pc="; while (*text) *p++ = *text++;
    p = append_hex(p, pc); *p++ = '\n';
    if (crash_fd >= 0) { ssize_t result = write(crash_fd, message, (size_t)(p-message)); (void)result; }
    for (unsigned i=0; i<sizeof(signals)/sizeof(signals[0]); ++i) {
        if (signals[i] != sig) continue;
        sigaction(sig, &previous[i], NULL);
        if (previous[i].sa_handler != SIG_DFL && previous[i].sa_handler != SIG_IGN) {
            if (previous[i].sa_flags & SA_SIGINFO) previous[i].sa_sigaction(sig, info, context);
            else previous[i].sa_handler(sig);
            return;
        }
        struct sigaction action = {0}; action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask); sigaction(sig, &action, NULL);
        raise(sig); /* blocked until this handler returns; retain signal death */
        return;
    }
}
void DRErrorLogStop(void) {
    for (unsigned i=0; i<5; ++i) if (installed[i]) {
        struct sigaction current;
        if (sigaction(signals[i], NULL, &current)==0 && (current.sa_flags & SA_SIGINFO)
            && current.sa_sigaction==record_signal) sigaction(signals[i], &previous[i], NULL);
        installed[i]=0;
    }
    if (crash_fd >= 0) close(crash_fd);
    crash_fd=-1;
    pthread_mutex_lock(&log_lock);
    if (log_fd >= 0) close(log_fd);
    log_fd=-1; written=0;
    pthread_mutex_unlock(&log_lock);
}
void DRErrorLogStart(void) {
    DRErrorLogStop();
    pthread_mutex_lock(&log_lock);
    const char* directory=getenv("DETHRACE_ERROR_DIRECTORY");
    if (directory && *directory) {
        int n=snprintf(log_path,sizeof(log_path),"%s/engine.log",directory);
        if (n>0 && (size_t)n<sizeof(log_path)) {
            snprintf(previous_path,sizeof(previous_path),"%s.previous",log_path);
            log_fd=open(log_path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
            char path[4096]; n=snprintf(path,sizeof(path),"%s/native-signal.txt",directory);
            if (n>0 && (size_t)n<sizeof(path)) crash_fd=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);
        }
    }
    pthread_mutex_unlock(&log_lock);
    if (crash_fd >= 0) {
        char maps_path[4096];
        int length=snprintf(maps_path,sizeof(maps_path),"%s/native-maps.txt",directory);
        if (length>0 && (size_t)length<sizeof(maps_path)) {
            int input=open("/proc/self/maps",O_RDONLY|O_CLOEXEC);
            int output=open(maps_path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
            if(input>=0 && output>=0) {
                char bytes[4096]; size_t left=128u*1024u; ssize_t count;
                while(left && (count=read(input,bytes,left<sizeof(bytes)?left:sizeof(bytes)))>0) {
                    ssize_t copied=write(output,bytes,(size_t)count); (void)copied; left-=(size_t)count;
                }
            }
            if(input>=0)close(input);
            if(output>=0)close(output);
        }
        struct sigaction action={0}; action.sa_sigaction=record_signal;
        action.sa_flags=SA_SIGINFO|SA_ONSTACK; sigemptyset(&action.sa_mask);
        for (unsigned i=0;i<5;++i)
            if (sigaction(signals[i],&action,&previous[i])==0) installed[i]=1;
    }
}
void DRErrorLogMessage(int priority, const char* prefix, const char* body) {
    if (priority < 4) return; /* omit per-call debug/trace traffic */
    pthread_mutex_lock(&log_lock);
    if (log_fd >= 0) {
        char line[1536]; struct timespec t; clock_gettime(CLOCK_REALTIME,&t);
        int n=snprintf(line,sizeof(line),"%lld.%03ld %s%s\n",(long long)t.tv_sec,t.tv_nsec/1000000,prefix,body);
        size_t count=n<0?0:(size_t)n<sizeof(line)?(size_t)n:sizeof(line)-1;
        if (written+count>LOG_LIMIT) {
            close(log_fd); rename(log_path,previous_path);
            log_fd=open(log_path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600); written=0;
        }
        if (log_fd >= 0) { ssize_t result=write(log_fd,line,count); if(result>0) written+=(size_t)result; }
    }
    pthread_mutex_unlock(&log_lock);
}
