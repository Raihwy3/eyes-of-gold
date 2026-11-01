/* NDSP output: channel 0 retains original wet/reverb and CPU overflow;
 * channels 1..23 combine independently enveloped dry voices on the DSP. */
#include <3ds.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "native_audio.h"
#include "native_dsp_mix.h"
extern void sysLogPrintf(int level,const char *fmt,...);
extern void sysFatalError(const char *fmt,...) __attribute__((noreturn));
#define BUFFERS 6
#define BUFFER_BYTES 4096
#define CHANNELS (NATIVE_DSP_DRY_CHANNELS+1)
static ndspWaveBuf waves[CHANNELS][BUFFERS];
static unsigned char *pcm,*silence;
static int ready,dsp_mode,dsp_requested=1;
static LightLock audio_lock;
static volatile int stopping,producer_parked;
static unsigned pending[BUFFERS],callback_cursor;
static void audio_stop_boundary(void) {
    if(!__atomic_load_n(&stopping,__ATOMIC_ACQUIRE))return;
    __atomic_store_n(&producer_parked,1,__ATOMIC_RELEASE);
    threadExit(0);
}
int native_audio_shutdown(void) {
    __atomic_store_n(&stopping,1,__ATOMIC_RELEASE);
    LightLock_Lock(&audio_lock);
    if(ready) {
        if(dsp_mode)ndspSetCallback(NULL,NULL);
        for(unsigned ch=0;ch<(dsp_mode?CHANNELS:1);ch++)ndspChnWaveBufClear(ch);
        ndspExit();ready=0;
    }
    LightLock_Unlock(&audio_lock);
    return 1;
}
static uint32_t cursor,last_bytes;
static uint64_t submitted;
/* The SDK invokes this before updating all DSP channels. Publish every stem
 * of a block in one callback, including silence on inactive voice channels.
 * Never block on the app lock: ndspExit may be joining this worker. */
static void audio_dsp_callback(void *unused) {
    (void)unused;
    if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE) || LightLock_TryLock(&audio_lock))return;
    if(ready && !__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)) {
        while(__atomic_load_n(&pending[callback_cursor],__ATOMIC_ACQUIRE)) {
            for(unsigned ch=0;ch<CHANNELS;ch++)ndspChnWaveBufAdd(ch,&waves[ch][callback_cursor]);
            __atomic_store_n(&pending[callback_cursor],0,__ATOMIC_RELEASE);
            callback_cursor=(callback_cursor+1)%BUFFERS;
        }
    }
    LightLock_Unlock(&audio_lock);
}
void native_audio_configure(const char *directory,int mixer) {
    char path[512];dsp_requested=mixer!=0;
    if(!directory)return;
    int n=snprintf(path,sizeof path,"%s/cpu-audio.flag",directory);
    if(n<0 || (unsigned)n>=sizeof path)return;
    FILE *f=fopen(path,"rb");if(f){fclose(f);dsp_requested=0;}
}
int native_audio_ready(void) {return ready;}
int native_audio_init(void) {
    LightLock_Init(&audio_lock);
    native_dsp_mix_init(0);
    Result result=ndspInit();
    if (R_FAILED(result)) {
        sysLogPrintf(1,"NDSP unavailable (%08lx); original sound remains disabled",(unsigned long)result);return 0;
    }
    dsp_mode=dsp_requested;
    pcm=linearAlloc((BUFFERS*(dsp_mode?CHANNELS:1)+(dsp_mode?1:0))*BUFFER_BYTES);
    if(!pcm && dsp_mode){dsp_mode=0;pcm=linearAlloc(BUFFERS*BUFFER_BYTES);}
    if(dsp_mode && !native_dsp_mix_init(1)) {
        dsp_mode=0;linearFree(pcm);pcm=linearAlloc(BUFFERS*BUFFER_BYTES);
    }
    if(!pcm){ndspExit();sysLogPrintf(1,"NDSP buffer allocation failed; sound disabled");return 0;}
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    for(unsigned ch=0;ch<(dsp_mode?CHANNELS:1);ch++) {
        ndspChnReset(ch);ndspChnSetFormat(ch,NDSP_FORMAT_STEREO_PCM16);
        ndspChnSetRate(ch,22050.0f);ndspChnSetInterp(ch,NDSP_INTERP_LINEAR);
        /* Original main-bus mix multiplies dry by 0x7fff before adding wet. */
        float gain=ch?32767.0f/32768.0f:1.0f;
        float mix[12]={gain,gain};ndspChnSetMix(ch,mix);
    }
    if(dsp_mode) {
        silence=pcm+BUFFERS*CHANNELS*BUFFER_BYTES;memset(silence,0,BUFFER_BYTES);DSP_FlushDataCache(silence,BUFFER_BYTES);
    }
    ready=1;
    if(dsp_mode)ndspSetCallback(audio_dsp_callback,NULL);
    sysLogPrintf(2,"Audio output: NDSP 22050 Hz stereo; %s",dsp_mode?"23 DSP dry voices + original CPU reverb/overflow":"original CPU mixer");return 1;
}
uint32_t audioGetAiLengthBytes(void) {
    audio_stop_boundary();
    LightLock_Lock(&audio_lock);
    if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)){LightLock_Unlock(&audio_lock);audio_stop_boundary();}
    if(!ready){LightLock_Unlock(&audio_lock);return 0;}
    uint32_t queued=0;
    for(unsigned i=0;i<BUFFERS;i++) {
        unsigned status=*(volatile uint8_t *)&waves[0][i].status;
        if(dsp_mode && __atomic_load_n(&pending[i],__ATOMIC_ACQUIRE))queued+=waves[0][i].nsamples*4;
        else if(status==NDSP_WBUF_QUEUED)queued+=waves[0][i].nsamples*4;
        else if(status==NDSP_WBUF_PLAYING) {
            uint32_t pos=ndspChnGetSamplePos(0);
            if(pos<waves[0][i].nsamples)queued+=(waves[0][i].nsamples-pos)*4;
        }
    }
    queued=queued>4096?queued-4096:0;
    uint32_t result=queued>last_bytes?last_bytes:queued;
    LightLock_Unlock(&audio_lock);return result;
}
static int audio_slot_busy(unsigned slot) {
    if(dsp_mode && __atomic_load_n(&pending[slot],__ATOMIC_ACQUIRE))return 1;
    for(unsigned ch=0;ch<(dsp_mode?CHANNELS:1);ch++) {
        unsigned status=*(volatile uint8_t *)&waves[ch][slot].status;
        if(status==NDSP_WBUF_QUEUED || status==NDSP_WBUF_PLAYING)return 1;
    }
    return 0;
}
void audioSetNextBuffer(const int16_t *buffer,uint32_t bytes) {
    audio_stop_boundary();
    if(!buffer || !bytes || (bytes&3) || bytes>BUFFER_BYTES)
        sysFatalError("Invalid native audio submission: %u bytes",(unsigned)bytes);
    while(audio_slot_busy(cursor)){audio_stop_boundary();svcSleepThread(1000000);}
    LightLock_Lock(&audio_lock);
    if(__atomic_load_n(&stopping,__ATOMIC_ACQUIRE)){LightLock_Unlock(&audio_lock);audio_stop_boundary();}
    if(!ready){LightLock_Unlock(&audio_lock);return;}
    const struct NativeDspMixJob *job=dsp_mode?native_dsp_mix_peek(buffer,bytes/4):NULL;
    unsigned char *slot_base=pcm+cursor*(dsp_mode?CHANNELS:1)*BUFFER_BYTES;
    unsigned flush_bytes=0;
    for(unsigned ch=0;ch<(dsp_mode?CHANNELS:1);ch++) {
        ndspWaveBuf *wave=&waves[ch][cursor];memset(wave,0,sizeof *wave);
        const void *source=ch?NULL:buffer;
        if(ch && job && (job->mask&(1u<<(ch-1))))source=job->pcm+(ch-1)*NATIVE_DSP_MIX_FRAMES*2;
        unsigned char *out=slot_base+ch*bytes;
        if(source){memcpy(out,source,bytes);flush_bytes=(ch+1)*bytes;wave->data_vaddr=out;}
        else wave->data_vaddr=silence;
        wave->nsamples=bytes/4;
    }
    if(flush_bytes)DSP_FlushDataCache(slot_base,flush_bytes);
    native_dsp_mix_release(buffer);
    if(dsp_mode)__atomic_store_n(&pending[cursor],1,__ATOMIC_RELEASE);
    else ndspChnWaveBufAdd(0,&waves[0][cursor]);
    last_bytes=bytes;submitted+=bytes;cursor=(cursor+1)%BUFFERS;
    LightLock_Unlock(&audio_lock);
    static int first=1;
    if(first){sysLogPrintf(2,"Audio submitted first block (%u bytes; DSP mixing=%d)",(unsigned)bytes,dsp_mode);first=0;}
}
uint64_t audioDumpBytePos(void) {return submitted;}
