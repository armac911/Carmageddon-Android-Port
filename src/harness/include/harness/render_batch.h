#ifndef DR_RENDER_BATCH_H
#define DR_RENDER_BATCH_H
/* Only use across GPU-only passes sharing the exact colour/depth target.
 * End the batch before any CPU pixel access or changing the render target. */
#ifdef __ANDROID__
void DRBeginShadowBatch(void);
void DREndShadowBatch(void);
void DRDeferShadowBatch(void);
#else
#define DRBeginShadowBatch() ((void)0)
#define DREndShadowBatch() ((void)0)
#define DRDeferShadowBatch() DREndShadowBatch()
#endif
#endif
