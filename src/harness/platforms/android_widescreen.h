#ifndef HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H
#define HARNESS_PLATFORMS_ANDROID_WIDESCREEN_H
#ifdef __ANDROID__
/* Select only original 4:3 or exact 16:9; never derive FOV from device aspect. */
void DRAndroid_ConfigureAspect(int* width, int* height);
#endif
#endif
