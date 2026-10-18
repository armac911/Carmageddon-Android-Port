#ifndef DR_ANDROID_INPUT_H
#define DR_ANDROID_INPUT_H
#ifdef __ANDROID__
void DRAndroidUpdateTouch(void);
void DRAndroidGamepadActivity(unsigned timestamp);
int DRAndroidTouchKey(int key);
int DRAndroidSteering(void);
int DRAndroidDigitalSteering(void);
int DRAndroidNormalizeStick(int value);
void DRAndroidSetGamepadSteering(int value);
void DRAndroidSetGamepadButtons(unsigned mask);
int DRAndroidTakeGearChange(void);
void DRAndroidFramePresented(void);
void DRAndroidImpact(int collision, float speed);
void DRAndroidStartText(const char* text, int max_length);
int DRAndroidPollText(int slot);
void DRAndroidCloseText(void);
#endif
#endif
