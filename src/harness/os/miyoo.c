/* Experimental target: errors go to stderr and the launcher log. */
#include "harness/os.h"
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <termios.h>
void OS_InstallSignalHandler(char* program_name) { (void)program_name; }
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
