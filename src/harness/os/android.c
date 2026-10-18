#define _GNU_SOURCE
#include "harness/os.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android/log.h>

#define LOG_TAG "DethRace-native"

static void signal_handler(int sig, siginfo_t *si, void *ctx) {
    (void)si;
    (void)ctx;
    __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, "Caught signal %d", sig);
    _exit(128 + sig);
}

void OS_InstallSignalHandler(char *program_name) {
    (void)program_name;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

FILE *OS_fopen(const char *pathname, const char *mode) {
    FILE *f = fopen(pathname, mode);
    if (f != NULL) {
        return f;
    }
    char buffer[512];
    char buffer2[512];
    strncpy(buffer, pathname, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    strncpy(buffer2, pathname, sizeof(buffer2) - 1);
    buffer2[sizeof(buffer2) - 1] = '\0';
    char *pDirName = dirname(buffer);
    char *pBaseName = basename(buffer2);
    DIR *pDir = opendir(pDirName);
    if (pDir == NULL) {
        return NULL;
    }
    for (struct dirent *pDirent = readdir(pDir); pDirent != NULL; pDirent = readdir(pDir)) {
        if (strcasecmp(pBaseName, pDirent->d_name) == 0) {
            char joined[1024];
            snprintf(joined, sizeof(joined), "%s/%s", pDirName, pDirent->d_name);
            f = fopen(joined, mode);
            break;
        }
    }
    closedir(pDir);
    return f;
}

size_t OS_ConsoleReadPassword(char *pBuffer, size_t pBufferLen) {
    if (pBufferLen > 0) {
        pBuffer[0] = '\0';
    }
    return 0;
}
