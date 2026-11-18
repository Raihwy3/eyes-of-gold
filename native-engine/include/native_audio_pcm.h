#ifndef EOG_NATIVE_AUDIO_PCM_H
#define EOG_NATIVE_AUDIO_PCM_H
#include <stdint.h>
/* One immutable cache mounted before audio threads. Unknown contexts retain
 * original decoding; predictor identity includes every coefficient byte. */
int native_audio_pcm_mount(const char *directory);
int native_audio_pcm_book(const void *coefficients,unsigned bytes);
int native_audio_pcm_frame(int book,const unsigned char *frame,int16_t prev2,int16_t prev1,int16_t *output);
void native_audio_pcm_stats(uint32_t *hits,uint32_t *misses);
#endif
