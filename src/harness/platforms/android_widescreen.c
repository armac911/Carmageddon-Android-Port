#ifdef __ANDROID__
#include "android_widescreen.h"
#include <stdlib.h>
#include <string.h>
#include "common/grafdata.h"
#include "harness/trace.h"
#include "pd/sys.h"

void DRAndroid_ConfigureAspect(int* width, int* height) {
    const char* wide = getenv("DETHRACE_WIDESCREEN_16_9");
    /* Exact 16:9 with an aligned row stride. Keep the original low-res
     * interface spec intact; it is doubled and centered at presentation. */
    if (!((*width == 640 && *height == 480) || (*width == 864 && *height == 486)))
        return;
    int enabled = wide && strcmp(wide, "1") == 0;
    *width = enabled ? 864 : 640;
    *height = enabled ? 486 : 480;
    gGraf_specs[1].total_width = gGraf_specs[1].phys_width = gGraf_specs[1].row_bytes = *width;
    gGraf_specs[1].total_height = gGraf_specs[1].phys_height = *height;
    gGraf_data[1].width = *width;
    gGraf_data[1].height = *height;
    LOG_INFO("Android: selected race canvas %dx%d", *width, *height);
}
#endif
