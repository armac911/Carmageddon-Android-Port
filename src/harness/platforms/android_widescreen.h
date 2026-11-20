#ifndef HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H
#define HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H
#ifdef __ANDROID__
/* Race canvas height in every wide mode; the 640x480 HUD sits centred in it. */
#define DR_ANDROID_WIDE_HEIGHT 486

/* Select the race canvas: original 4:3 (640x480), exact 16:9 (864x486) or
 * "fill", where the width follows the device surface aspect at height 486.
 * surface_w/surface_h are the SDL drawable size (either orientation); they
 * are only used by fill mode, which falls back to 16:9 when they are unknown.
 * The vertical FOV is unchanged in every mode; extra width only adds view. */
void DRAndroid_ConfigureAspect(int* width, int* height, int surface_w, int surface_h);
#endif
#endif
