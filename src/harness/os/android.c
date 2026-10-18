#define _GNU_SOURCE
#include "harness/os.h"
#include "../platforms/error_log.h"

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

/* Keep Android's fatal-signal reporting; optional capture chains to it. */
void OS_InstallSignalHandler(char *program_name) {
    (void)program_name;
    DRErrorLogStart();
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
