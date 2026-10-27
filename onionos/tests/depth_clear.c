#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "brender/brender.h"
static br_uint_16 pixels[200][336];
int main(void){
    br_pixelmap pm={0};pm.type=BR_PMT_DEPTH_16;pm.width=320;pm.height=200;
    pm.row_bytes=sizeof(pixels[0]);pm.pixels=pixels;
    BrPixelmapRectangleFill(&pm,0,0,320,200,0xffffffff);
    /* Original byte-only shortcut fails at x=160, including the whole right half. */
    for(int y=0;y<200;y++)for(int x=0;x<336;x++)assert(pixels[y][x]==(x<320?0xffff:0));
    memset(pixels,0,sizeof(pixels));pm.width=8;pm.height=4;pm.base_x=3;pm.base_y=2;
    pm.origin_x=2;pm.origin_y=1;
    BrPixelmapRectangleFill(&pm,-4,-2,7,5,0x1234);
    for(int y=0;y<200;y++)for(int x=0;x<336;x++)
        assert(pixels[y][x]==(y>=2&&y<6&&x>=3&&x<8?0x1234:0));
    BrPixelmapRectangleFill(&pm,20,20,3,3,0xffff); /* Fully outside: no writes. */
    assert(pixels[0][0]==0&&pixels[5][7]==0x1234&&pixels[6][3]==0);
    puts("PASS real depth rectangle clear: full 320px row, 16-bit colour, stride, subview bases, origin and clipping");
}
