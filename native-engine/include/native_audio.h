#ifndef NATIVE_AUDIO_H
#define NATIVE_AUDIO_H
#include <stdint.h>
void native_audio_configure(const char *directory,int mixer);
int native_audio_init(void);
int native_audio_ready(void);
int native_audio_shutdown(void);
int native_audio_bank_convert(void *blob,uint32_t bytes);
void native_audio_bank_fixup(void *blob,uint32_t bytes,uint32_t table);
int native_audio_register_raw(const void *blob,uint32_t table);
void native_audio_swap_samples(uint32_t address,void *data,uint32_t bytes);
#endif
