#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t tU8;
typedef int16_t br_int_16;
typedef struct {
    int width, height, row_bytes, origin_x, origin_y;
    void* pixels;
} br_pixelmap;
static br_pixelmap* gCurrent_conversion_table;
#define LOG_TRACE(...) ((void)0)

/* The test runner inserts the actual engine function here. */
/* ENGINE_FUNCTION */

int main(void) {
    uint8_t pixels[12] = {1,2,0,4, 5,0,7,8, 9,10,11,12};
    uint8_t buffer[8 * 6];
    br_pixelmap src = {4,3,4,2,1,pixels};
    br_pixelmap dst = {6,6,8,0,0,buffer};
    memset(buffer, 99, sizeof(buffer));
    DRPixelmapRectangleMaskedCopy(&dst,1,1,&src,0,0,4,3);
    /* Cursor images can retain a nonzero BRender origin from the PIX file.
     * This engine helper takes raw sprite coordinates, not origin-relative
     * BRender coordinates. All rows must be read from the requested region. */
    for (int y=0;y<3;++y) for (int x=0;x<4;++x)
        assert(buffer[(y+1)*8+x+1] == (pixels[y*4+x] ? pixels[y*4+x] : 99));
    for (int y=0;y<6;++y) assert(buffer[y*8+6] == 99 && buffer[y*8+7] == 99);

    memset(buffer,99,sizeof(buffer));
    DRPixelmapRectangleMaskedCopy(&dst,-2,-1,&src,0,0,4,3);
    assert(buffer[0] == 7 && buffer[1] == 8);
    assert(buffer[8] == 11 && buffer[9] == 12);
    assert(buffer[2] == 99 && buffer[16] == 99);

    memset(buffer,99,sizeof(buffer));
    DRPixelmapRectangleMaskedCopy(&dst,5,5,&src,0,0,4,3);
    assert(buffer[5*8+5] == 1);
    assert(buffer[5*8+6] == 99);

    uint8_t table[256];
    for (int i=0;i<256;++i) table[i] = (uint8_t)(255-i);
    br_pixelmap conversion = {256,1,256,0,0,table};
    gCurrent_conversion_table = &conversion;
    memset(buffer,99,sizeof(buffer));
    DRPixelmapRectangleMaskedCopy(&dst,0,0,&src,0,0,4,3);
    assert(buffer[0] == 254 && buffer[1] == 253 && buffer[2] == 99 && buffer[3] == 251);
    puts("PASS: actual engine masked-copy routine: sprite origins, transparency, padded rows, edge clipping, palette conversion");
    return 0;
}
