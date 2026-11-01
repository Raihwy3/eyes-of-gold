#include <3ds.h>
#include "native_audio.h"
#include "native_dsp_mix.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
extern void audioSetNextBuffer(const int16_t *,uint32_t);
extern uint32_t audioGetAiLengthBytes(void);
static int lock_held,channel_count,initializations,exited,allocations,fail_linear,fail_stage,fail_init,added,cleared,sleeps;
static void (*callback)(void *);static void *callback_data;
static ndspWaveBuf *submitted[24][64];static unsigned channel_added[24];
static float channel_gain[24];static jmp_buf stopped;
void *audio_test_calloc(size_t n,size_t s){return fail_stage?NULL:calloc(n,s);}
void LightLock_Init(LightLock *l){*l=0;}
void LightLock_Lock(LightLock *l){(void)l;assert(!lock_held);lock_held=1;}
void LightLock_Unlock(LightLock *l){(void)l;assert(lock_held);lock_held=0;}
int LightLock_TryLock(LightLock *l){(void)l;if(lock_held)return 1;lock_held=1;return 0;}
_Noreturn void threadExit(int code){assert(!code);longjmp(stopped,1);}
void svcSleepThread(int64_t ns){assert(ns>0);sleeps++;for(unsigned c=0;c<(unsigned)channel_count;c++)for(unsigned j=0;j<channel_added[c];j++)submitted[c][j]->status=NDSP_WBUF_DONE;}
Result ndspInit(void){initializations++;return fail_init?-1:0;}
void ndspExit(void){assert(!callback && (lock_held || !channel_count));exited++;}
void *linearAlloc(size_t bytes){allocations++;return fail_linear && (allocations==1 || fail_linear==2)?NULL:calloc(1,bytes);}
void linearFree(void *p){free(p);}
void ndspSetOutputMode(int mode){assert(mode==NDSP_OUTPUT_STEREO);}
void ndspChnReset(int ch){assert(ch==channel_count);channel_count++;}
void ndspChnSetFormat(int ch,int f){assert(ch<channel_count && f==NDSP_FORMAT_STEREO_PCM16);}
void ndspChnSetRate(int ch,float rate){assert(ch<channel_count && rate==22050.f);}
void ndspChnSetInterp(int ch,int mode){assert(ch<channel_count && mode==NDSP_INTERP_LINEAR);}
void ndspChnSetMix(int ch,float *mix){channel_gain[ch]=mix[0];assert(mix[0]==mix[1]);for(int i=2;i<12;i++)assert(mix[i]==0);}
void ndspSetCallback(void (*cb)(void *),void *data){callback=cb;callback_data=data;}
void ndspChnWaveBufAdd(int ch,ndspWaveBuf *wave){assert(lock_held && ch<channel_count && wave->status!=NDSP_WBUF_QUEUED && wave->status!=NDSP_WBUF_PLAYING);submitted[ch][channel_added[ch]++]=wave;wave->status=NDSP_WBUF_QUEUED;added++;}
void ndspChnWaveBufClear(int ch){assert(lock_held && ch==cleared);cleared++;}
uint32_t ndspChnGetSamplePos(int ch){assert(ch==0);return 0;}
void DSP_FlushDataCache(const void *p,uint32_t bytes){assert(p && bytes && bytes<=24*4096);}
void sysLogPrintf(int level,const char *fmt,...){(void)level;(void)fmt;}
_Noreturn void sysFatalError(const char *fmt,...){(void)fmt;abort();}
static void pump(void){if(callback){int old=added;callback(callback_data);assert((added-old)%24==0);}}
int main(int argc,char **argv) {
    assert(argc==3);int cpu=!strcmp(argv[1],"cpu") || !strcmp(argv[1],"cpu-setting");fail_linear=!strcmp(argv[1],"linear-fail");fail_stage=!strcmp(argv[1],"stage-fail");
    fail_init=!strcmp(argv[1],"ndsp-fail");if(!strcmp(argv[1],"all-linear-fail"))fail_linear=2;
    native_audio_configure(argv[2],strcmp(argv[1],"cpu-setting")!=0);
    if(fail_init || fail_linear==2) {
        assert(!native_audio_init() && !native_audio_ready());
        native_dsp_mix_begin(argv,128);assert(!native_dsp_mix_target(123,0,128));native_dsp_mix_end();
        assert(native_audio_shutdown() && !channel_count && !callback && exited==!fail_init);
        printf("PASS actual DSP unavailable (%s): clean muted boot, no voice diversion and safe shutdown\n",argv[1]);return 0;
    }
    assert(native_audio_init() && native_audio_ready());
    int dsp=!(cpu||fail_linear||fail_stage);assert(channel_count==(dsp?24:1));assert(initializations==1 && channel_gain[0]==1.f);
    int16_t base[1024*2];for(unsigned i=0;i<2048;i++)base[i]=(int16_t)(i-1024);
    native_dsp_mix_begin(base,128);int16_t *dry=native_dsp_mix_target(123,0,128);
    assert((dry!=NULL)==dsp);if(dry)for(unsigned i=0;i<256;i++)dry[i]=(int16_t)(500-i);
    native_dsp_mix_advance(128);native_dsp_mix_end();
    audioSetNextBuffer(base,512);assert(!native_dsp_mix_peek(base,128));
    if(dsp) {
        assert(!added);lock_held=1;pump();assert(!added && lock_held);lock_held=0;
        pump();assert(added==24);
        assert(channel_gain[1]==32767.f/32768.f);
        assert(!memcmp(submitted[1][0]->data_vaddr,(int16_t[2]){500,499},4));
        for(unsigned c=2;c<24;c++)for(unsigned i=0;i<256;i++)assert(!((const int16_t *)submitted[c][0]->data_vaddr)[i]);
    }else assert(added==1);
    assert(!memcmp(submitted[0][0]->data_vaddr,base,512));
    // Queue through wraparound. Every channel has the same lengths and order;
    // reusing a staging job must never alter already published linear data.
    for(unsigned block=1;block<10;block++) {
        native_dsp_mix_begin(base,1024);dry=native_dsp_mix_target(123,17,900);
        if(dry)for(unsigned i=0;i<1800;i++)dry[i]=(int16_t)block;
        native_dsp_mix_advance(1024);native_dsp_mix_end();
        audioSetNextBuffer(base,4096);pump();
        for(unsigned c=0;c<(unsigned)channel_count;c++)assert(channel_added[c]==block+1 && submitted[c][block]->nsamples==1024);
        assert(audioGetAiLengthBytes()<=4096);
    }
    assert(sleeps>0); /* Pending/active ring blocks were not overwritten. */
    void (*old_callback)(void *)=callback;
    assert(native_audio_shutdown() && !native_audio_ready() && !callback && cleared==channel_count && exited==1);
    if(old_callback)old_callback(NULL);assert(!lock_held);
    if(!setjmp(stopped)){audioGetAiLengthBytes();assert(0);} /* Producer parks without waiting on DSP. */
    printf("PASS actual DSP audio backend (%s): synchronized channel batches, PCM/silence/lengths, six-slot wrap, allocation fallback and bounded shutdown\n",argv[1]);
}
