#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t tU8;
typedef uint16_t tU16;
typedef struct { void* pixels; int row_bytes; } br_pixelmap;
#define LOG_TRACE(...)
/* ENGINE_FUNCTION */
int main(void) {
    static uint8_t src[864*486], dst[864*486];
    br_pixelmap s = {src, 864}, d = {dst, 864};
    for (int y=0; y<200; ++y) for (int x=0; x<320; ++x)
        src[y*864+x] = (x+y)%254+1;
    DRPixelmapDoubledCopy(&d, &s, 320, 200, 112, 43);
    for (int y=0; y<486; ++y) for (int x=0; x<864; ++x) {
        int inside = x>=112 && x<752 && y>=43 && y<443;
        int want = inside ? src[((y-43)/2)*864+(x-112)/2] : 0;
        assert(dst[y*864+x] == want);
    }
    memset(dst,0,sizeof(dst));
    s.row_bytes = d.row_bytes = 640;
    memset(src,23,sizeof(src));
    DRPixelmapDoubledCopy(&d,&s,320,200,0,40);
    assert(dst[40*640] == 23 && dst[439*640+639] == 23);
    assert(dst[39*640] == 0 && dst[440*640] == 0);
    puts("PASS: actual doubled-copy centered 16:9 menu, source stride, full margins, 4:3 regression");
}
