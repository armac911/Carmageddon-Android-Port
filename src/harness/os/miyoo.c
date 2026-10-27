/* Experimental target: errors go to stderr and the launcher log. */
#define _GNU_SOURCE
#include "harness/os.h"
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <ucontext.h>

static union { long double alignment; unsigned char bytes[65536]; } crash_stack;
static void crash_write(const char* data, size_t len) {
    while (len) {
        ssize_t n=write(STDERR_FILENO,data,len);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        data+=n;len-=(size_t)n;
    }
}
/* Only write/open/read/close/_exit and local arithmetic are used in the handler.
 * No printf, malloc, unwinding or dereference of a possibly-corrupt stack. */
static char* crash_hex(char* p, uintptr_t value) {
    static const char digits[]="0123456789abcdef";
    *p++='0';*p++='x';
    for(int i=(int)(sizeof(value)*2)-1;i>=0;i--)*p++=digits[(value>>(i*4))&15];
    return p;
}
static char* crash_field(char* p,const char* label,uintptr_t value) {
    while(*label)*p++=*label++;
    return crash_hex(p,value);
}
static void crash_handler(int sig,siginfo_t* info,void* context) {
    char line[1024];char* p=line;
    p=crash_field(p,"MIYOO_CRASH signal=",(uintptr_t)sig);
    p=crash_field(p," code=",(uintptr_t)info->si_code);
    p=crash_field(p," fault=",(uintptr_t)info->si_addr);
    ucontext_t* uc=context;
#if defined(__arm__)
    p=crash_field(p," pc=",uc->uc_mcontext.arm_pc);
    p=crash_field(p," lr=",uc->uc_mcontext.arm_lr);
    p=crash_field(p," sp=",uc->uc_mcontext.arm_sp);
    p=crash_field(p," fp=",uc->uc_mcontext.arm_fp);
    p=crash_field(p," r0=",uc->uc_mcontext.arm_r0);
    p=crash_field(p," r1=",uc->uc_mcontext.arm_r1);
    p=crash_field(p," r2=",uc->uc_mcontext.arm_r2);
    p=crash_field(p," r3=",uc->uc_mcontext.arm_r3);
#elif defined(__x86_64__)
    p=crash_field(p," pc=",(uintptr_t)uc->uc_mcontext.gregs[REG_RIP]);
    p=crash_field(p," sp=",(uintptr_t)uc->uc_mcontext.gregs[REG_RSP]);
#else
    (void)uc;
#endif
    *p++='\n';crash_write(line,(size_t)(p-line));
    static const char heading[]="MIYOO_CRASH maps begin\n";
    crash_write(heading,sizeof(heading)-1);
    int fd=open("/proc/self/maps",O_RDONLY);
    if(fd>=0){
        ssize_t n;size_t total=0;
        while(total<65536 && (n=read(fd,line,sizeof(line)))>0){crash_write(line,(size_t)n);total+=(size_t)n;}
        close(fd);
    }
    static const char ending[]="MIYOO_CRASH maps end\n";
    crash_write(ending,sizeof(ending)-1);
    _exit(128+sig);
}
void OS_InstallSignalHandler(char* program_name) {
    (void)program_name;
    stack_t stack={0};stack.ss_sp=crash_stack.bytes;stack.ss_size=sizeof(crash_stack.bytes);
    if(sigaltstack(&stack,NULL)){perror("Miyoo sigaltstack");return;}
    struct sigaction action={0};action.sa_sigaction=crash_handler;
    sigemptyset(&action.sa_mask);action.sa_flags=SA_SIGINFO|SA_ONSTACK;
    const int signals[]={SIGSEGV,SIGBUS,SIGILL,SIGFPE,SIGABRT};
    for(unsigned i=0;i<sizeof(signals)/sizeof(signals[0]);i++)
        if(sigaction(signals[i],&action,NULL))perror("Miyoo sigaction");
    fputs("Miyoo crash capture installed: signal/fault/PC/LR/maps\n",stderr);
}
FILE* OS_fopen(const char* pathname, const char* mode) {
    FILE* f = fopen(pathname, mode);
    if (f != NULL) {
        return f;
    }
    char buffer[512];
    char buffer2[512];
    strcpy(buffer, pathname);
    strcpy(buffer2, pathname);
    char* pDirName = dirname(buffer);
    char* pBaseName = basename(buffer2);
    DIR* pDir = opendir(pDirName);
    if (pDir == NULL) {
        return NULL;
    }
    for (struct dirent* pDirent = readdir(pDir); pDirent != NULL; pDirent = readdir(pDir)) {
        if (strcasecmp(pBaseName, pDirent->d_name) == 0) {
            strcat(pDirName, "/");
            strcat(pDirName, pDirent->d_name);
            f = fopen(pDirName, mode);
            break;
        }
    }
    closedir(pDir);
    return f;
}

size_t OS_ConsoleReadPassword(char* pBuffer, size_t pBufferLen) {
    struct termios old, new;
    char c;
    size_t len;

    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new);

    len = 0;
    pBuffer[len] = '\0';
    while (1) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 0x7f) {
                if (len > 0) {
                    len--;
                    pBuffer[len] = '\0';
                    printf("\033[1D \033[1D");
                    fflush(stdout);
                    continue;
                }
            } else if (c == '\r' || c == '\n') {
                printf("\n");
                fflush(stdout);
                break;
            } else if (len < pBufferLen - 1) {
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                    pBuffer[len] = c;
                    printf("*");
                    fflush(stdout);
                    len++;
                    pBuffer[len] = '\0';
                }
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    return len;
}
