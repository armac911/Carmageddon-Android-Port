#include "harness/trace.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

int harness_debug_level = 4;

#ifdef __ANDROID__
static void strip_ansi(char* s) {
    char* w = s;
    for (char* r = s; *r;) {
        if (*r == '\033' && r[1] == '[') {
            r += 2;
            while (*r && *r != 'm') r++;
            if (*r) r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}
#endif

void debug_printf(const char* fmt, const char* fn, const char* fmt2, ...) {
    va_list ap;

#ifdef __ANDROID__
    char prefix[256];
    char body[1024];
    snprintf(prefix, sizeof(prefix), fmt, fn);
    strip_ansi(prefix);
    va_start(ap, fmt2);
    vsnprintf(body, sizeof(body), fmt2, ap);
    va_end(ap);
    int prio = ANDROID_LOG_INFO;
    if (strstr(prefix, "[PANIC]")) prio = ANDROID_LOG_FATAL;
    else if (strstr(prefix, "[WARN]")) prio = ANDROID_LOG_WARN;
    else if (strstr(prefix, "[DEBUG]")) prio = ANDROID_LOG_DEBUG;
    __android_log_print(prio, "dethrace", "%s%s", prefix, body);
#else
    printf(fmt, fn);

    va_start(ap, fmt2);
    vprintf(fmt2, ap);
    va_end(ap);

    puts("\033[0m");
#endif
}

void debug_print_vector3(const char* fmt, const char* fn, char* msg, br_vector3* v) {
    printf(fmt, fn);
    printf("%s %f, %f, %f\n", msg, v->v[0], v->v[1], v->v[2]);
    puts("\033[0m");
}

void debug_print_matrix34(const char* fmt, const char* fn, char* msg, br_matrix34* m) {
    printf(fmt, fn);
    printf("matrix34 \"%s\"\n", msg);
    for (int i = 0; i < 4; i++) {
        printf("  %f, %f, %f\n", m->m[i][0], m->m[i][1], m->m[i][2]);
    }
    puts("\033[0m");
}

void debug_print_matrix4(const char* fmt, const char* fn, char* msg, br_matrix4* m) {
    printf(fmt, fn);
    printf("matrix34 \"%s\"\n", msg);
    for (int i = 0; i < 4; i++) {
        printf("  %f, %f, %f, %f\n", m->m[i][0], m->m[i][1], m->m[i][2], m->m[i][3]);
    }
    puts("\033[0m");
}

// int count_open_fds(void) {
//     DIR* dp = opendir("/dev/fd/");
//     struct dirent* de;
//     int count = -3; // '.', '..', dp

//     if (dp == NULL)
//         return -1;

//     while ((de = readdir(dp)) != NULL)
//         count++;

//     (void)closedir(dp);

//     return count;
// }
