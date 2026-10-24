#ifndef DR_LOCALIZATION_H
#define DR_LOCALIZATION_H
#include "dr_types.h"
void DRLanguageInit(const char* language);
int DRLanguageActive(void);
char* DRLocalize(const char* english);
char* DRLocalizeMisc(int index, char* original);
int DRLanguageHasUtf8(const char* text);
int DRLanguageNext(const unsigned char** text);
int DRLanguageBase(int codepoint);
int DRLanguageTextWidth(tDR_font* font, const char* text, int clever);
void DRLanguageDrawText(br_pixelmap* target, int x, int y, tDR_font* font, const char* text, int right, int clever);
void DRLanguageInterface(tInterface_spec* spec, int selection);
#endif
