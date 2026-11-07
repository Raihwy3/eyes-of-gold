#ifndef EOG_TEST_DSP_3DS_H
#define EOG_TEST_DSP_3DS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint32_t u32;typedef int32_t Result;typedef int LightLock;
#define R_FAILED(r) ((r)<0)
#define NDSP_OUTPUT_STEREO 1
#define NDSP_FORMAT_STEREO_PCM16 10
#define NDSP_INTERP_LINEAR 1
enum {NDSP_WBUF_FREE,NDSP_WBUF_QUEUED,NDSP_WBUF_PLAYING,NDSP_WBUF_DONE};
typedef struct ndspWaveBuf {const void *data_vaddr;uint32_t nsamples;unsigned char status;} ndspWaveBuf;
void LightLock_Init(LightLock *);void LightLock_Lock(LightLock *);void LightLock_Unlock(LightLock *);int LightLock_TryLock(LightLock *);
_Noreturn void threadExit(int);void svcSleepThread(int64_t);
void ndspUseComponent(const void *,uint32_t,uint16_t,uint16_t);void ndspAuxSetVolume(int,float);
Result ndspInit(void);void ndspExit(void);void *linearAlloc(size_t);void linearFree(void *);
void ndspSetOutputMode(int);void ndspChnReset(int);void ndspChnSetFormat(int,int);void ndspChnSetRate(int,float);void ndspChnSetInterp(int,int);void ndspChnSetMix(int,float *);
void ndspSetCallback(void (*)(void *),void *);void ndspChnWaveBufAdd(int,ndspWaveBuf *);void ndspChnWaveBufClear(int);uint32_t ndspChnGetSamplePos(int);void DSP_FlushDataCache(const void *,uint32_t);
#endif
