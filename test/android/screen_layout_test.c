#include "../../src/harness/platforms/screen_layout.h"
#include <assert.h>
#include <stdio.h>

static void check_point(DRScreenLayout* s, int wx, int wy, int menu,
    int want_x, int want_y, int want_inside) {
    int x, y;
    int inside = DRScreenMapPointer(s, wx, wy, menu, &x, &y);
    assert(x == want_x && y == want_y && inside == want_inside);
}

int main(void) {
    DRScreenLayout s = {0};
    /* S24 FE recording dimensions: the old build had 450px side bars. */
    assert(DRScreenLayoutInit(&s, 2340,1080,2340,1080,640,480,0));
    assert(s.x == 450 && s.y == 0 && s.w == 1440 && s.h == 1080);
    check_point(&s,1170,540,1,160,100,1);
    check_point(&s,450,90,1,0,0,1);
    check_point(&s,1170,0,1,160,0,0);
    check_point(&s,0,540,1,0,100,0);
    /* A button near the bottom must map to its actual low-res Y, not
     * the old erroneous scaling by 200/480. Forward projection is exact. */
    check_point(&s,810,900,1,80,180,1);
    /* Exact native 16:9, bounded on the wider S24 FE display. */
    assert(DRScreenLayoutInit(&s,2340,1080,2340,1080,864,486,0));
    assert(s.x == 210 && s.y == 0 && s.w == 1920 && s.h == 1080);
    check_point(&s,1170,540,0,432,243,1);
    check_point(&s,1170,540,1,160,100,1);
    check_point(&s,210,540,1,0,100,0);
    check_point(&s,0,540,0,0,243,0);
    assert(DRScreenLayoutInit(&s,1170,540,2340,1080,864,486,0));
    check_point(&s,585,270,1,160,100,1);
    /* A native 16:9 screen has no bars; a 4:3 tablet gets top/bottom bars. */
    assert(DRScreenLayoutInit(&s,1920,1080,1920,1080,864,486,0));
    assert(s.x == 0 && s.y == 0 && s.w == 1920 && s.h == 1080);
    assert(DRScreenLayoutInit(&s,1440,1080,1440,1080,864,486,0));
    assert(s.x == 0 && s.y == 135 && s.w == 1440 && s.h == 810);
    assert(DRScreenLayoutInit(&s,1280,1024,1280,1024,640,480,0));
    assert(s.x == 0 && s.y == 32 && s.w == 1280 && s.h == 960);
    check_point(&s,640,512,1,160,100,1);
    assert(!DRScreenLayoutInit(&s,0,1080,2340,1080,640,480,1));
    puts("PASS: 4:3 / 16:9 fixed aspects, S24 FE menu button alignment, menu margins, HiDPI, boundaries");
    return 0;
}
