#ifdef __ANDROID__
#include "android_widescreen.h"
#include <stdlib.h>
#include <string.h>
#include "common/grafdata.h"
#include "harness/trace.h"
#include "pd/sys.h"

/* Widest canvas accepted for fill mode (~2.67:1); keeps buffers bounded. */
#define DR_ANDROID_FILL_MAX_WIDTH 1296

enum { kAspect_classic, kAspect_wide, kAspect_fill };

static int selected_aspect(void) {
    const char* aspect = getenv("DETHRACE_ASPECT");
    if (aspect != NULL) {
        if (strcmp(aspect, "fill") == 0) return kAspect_fill;
        if (strcmp(aspect, "wide") == 0) return kAspect_wide;
        if (strcmp(aspect, "classic") == 0) return kAspect_classic;
    }
    /* Older launchers only export the 16:9 flag. */
    const char* wide = getenv("DETHRACE_WIDESCREEN_16_9");
    return wide && strcmp(wide, "1") == 0 ? kAspect_wide : kAspect_classic;
}

static int fill_width(int surface_w, int surface_h) {
    int long_side = surface_w > surface_h ? surface_w : surface_h;
    int short_side = surface_w > surface_h ? surface_h : surface_w;
    if (long_side <= 0 || short_side <= 0)
        return 864;
    long long w = ((long long)DR_ANDROID_WIDE_HEIGHT * long_side + short_side / 2) / short_side;
    /* Multiple of 8: keeps the engine's &~3 render-rect maths exact. The
     * presenter absorbs the sub-1% rounding without visible bars. */
    w = (w + 4) & ~7LL;
    if (w < 640) w = 640;
    if (w > DR_ANDROID_FILL_MAX_WIDTH) w = DR_ANDROID_FILL_MAX_WIDTH;
    return (int)w;
}

void DRAndroid_ConfigureAspect(int* width, int* height, int surface_w, int surface_h) {
    /* Only the high-resolution race spec is widened. Keep the original
     * low-res interface spec intact; it is doubled and centred at presentation. */
    if (!((*width == 640 && *height == 480) || *height == DR_ANDROID_WIDE_HEIGHT))
        return;
    switch (selected_aspect()) {
    case kAspect_fill:
        *width = fill_width(surface_w, surface_h);
        *height = DR_ANDROID_WIDE_HEIGHT;
        break;
    case kAspect_wide:
        *width = 864;
        *height = DR_ANDROID_WIDE_HEIGHT;
        break;
    default:
        *width = 640;
        *height = 480;
        break;
    }
    gGraf_specs[1].total_width = gGraf_specs[1].phys_width = gGraf_specs[1].row_bytes = *width;
    gGraf_specs[1].total_height = gGraf_specs[1].phys_height = *height;
    gGraf_data[1].width = *width;
    gGraf_data[1].height = *height;
    LOG_INFO("Android: selected race canvas %dx%d (surface %dx%d)", *width, *height, surface_w, surface_h);
}
#endif
