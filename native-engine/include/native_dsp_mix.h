#ifndef EOG_NATIVE_DSP_MIX_H
#define EOG_NATIVE_DSP_MIX_H
#include <stdint.h>
#define NATIVE_DSP_DRY_CHANNELS 23
#define NATIVE_DSP_MIX_FRAMES 1024
#define NATIVE_DSP_MIX_JOBS 3
struct NativeDspMixJob {const void *key;unsigned frames,offset,mask,sealed;unsigned end[NATIVE_DSP_DRY_CHANNELS];int16_t *pcm;};
int native_dsp_mix_init(int enabled);
void native_dsp_mix_begin(const void *key,unsigned frames);
void native_dsp_mix_advance(unsigned frames);
void native_dsp_mix_end(void);
int16_t *native_dsp_mix_target(uintptr_t voice,unsigned local_offset,unsigned frames);
const struct NativeDspMixJob *native_dsp_mix_peek(const void *key,unsigned frames);
void native_dsp_mix_release(const void *key);
void native_dsp_mix_stats(uint32_t *spans,uint32_t *samples,uint32_t *fallbacks);
#endif
