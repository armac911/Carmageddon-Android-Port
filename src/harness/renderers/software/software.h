#ifndef DR_SOFTWARE_H
#define DR_SOFTWARE_H
#include "harness/hooks.h"
void SWBegin(br_actor*, br_pixelmap*, br_pixelmap*);
void SWModel(br_actor*, br_model*, br_material*, br_token, br_matrix34);
void SWMaterial(br_material*);
void SWFlush(void);
void SWEnd(void);
void SWClear(void);
void SWTexture(br_pixelmap*);
void SWBufferModel(br_model*);
void SWViewport(int,int,int,int);
unsigned long SWTriangleCount(void);
#endif
