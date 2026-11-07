/* Audio-producer-owned sidecars follow the original three AudioInfo buffers.
 * Once copied to the backend's linear ring, a job can be reused. DSP callbacks
 * never read these jobs, allocate memory or touch original libaudio state. */
#include "native_dsp_mix.h"
#include <stdlib.h>
#include <string.h>
static struct NativeDspMixJob jobs[NATIVE_DSP_MIX_JOBS],*current;
static uintptr_t voices[NATIVE_DSP_DRY_CHANNELS];
static int16_t *storage;
static int active,external_fx;
static unsigned spans,samples,fallbacks;
int native_dsp_mix_init(int enabled) {
    active=0;external_fx=enabled==2;
    if(storage)return active=enabled!=0;
    if(!enabled)return 0;
    storage=calloc(NATIVE_DSP_MIX_JOBS*(NATIVE_DSP_DRY_CHANNELS+1)*NATIVE_DSP_MIX_FRAMES*2,sizeof *storage);
    if(!storage)return 0;
    for(unsigned j=0;j<NATIVE_DSP_MIX_JOBS;j++){jobs[j].pcm=storage+j*(NATIVE_DSP_DRY_CHANNELS+1)*NATIVE_DSP_MIX_FRAMES*2;jobs[j].wet_pcm=jobs[j].pcm+NATIVE_DSP_DRY_CHANNELS*NATIVE_DSP_MIX_FRAMES*2;}
    active=1;return 1;
}
void native_dsp_mix_begin(const void *key,unsigned frames) {
    current=NULL;if(!active || !storage || !key || !frames || frames>NATIVE_DSP_MIX_FRAMES)return;
    for(unsigned j=0;j<NATIVE_DSP_MIX_JOBS;j++)if(!jobs[j].key) {
        current=&jobs[j];current->key=key;current->frames=frames;current->offset=current->mask=current->sealed=current->wet_written=0;memset(current->end,0,sizeof current->end);return;
    }
    fallbacks++; /* Original CPU mixing remains valid for this entire block. */
}
void native_dsp_mix_advance(unsigned frames) {
    if(current && frames<=current->frames-current->offset)current->offset+=frames;
}
void native_dsp_mix_end(void){if(current)current->sealed=1;current=NULL;}
int16_t *native_dsp_mix_target(uintptr_t voice,unsigned offset,unsigned frames) {
    if(!current || !voice || !frames || offset>current->frames-current->offset || frames>current->frames-current->offset-offset)return NULL;
    int channel=-1;
    unsigned limit=NATIVE_DSP_DRY_CHANNELS-(external_fx!=0);
    for(unsigned i=0;i<limit;i++)if(voices[i]==voice){channel=i;break;}
    if(channel<0)for(unsigned i=0;i<limit;i++)if(!voices[i]){voices[i]=voice;channel=i;break;}
    if(channel<0){fallbacks++;return NULL;}
    unsigned absolute=current->offset+offset;
    if(absolute<current->end[channel]){fallbacks++;return NULL;}
    current->end[channel]=absolute+frames;
    int16_t *pcm=current->pcm+(unsigned)channel*NATIVE_DSP_MIX_FRAMES*2;
    if(!(current->mask&(1u<<channel))){memset(pcm,0,current->frames*4);current->mask|=1u<<channel;}
    spans++;samples+=frames;
    return pcm+(current->offset+offset)*2;
}
const struct NativeDspMixJob *native_dsp_mix_peek(const void *key,unsigned frames) {
    for(unsigned j=0;j<NATIVE_DSP_MIX_JOBS;j++)if(jobs[j].key==key && jobs[j].frames==frames && jobs[j].sealed)return &jobs[j];
    return NULL;
}
void native_dsp_mix_release(const void *key) {
    for(unsigned j=0;j<NATIVE_DSP_MIX_JOBS;j++)if(jobs[j].key==key){jobs[j].key=NULL;jobs[j].sealed=jobs[j].mask=0;}
}
void native_dsp_mix_stats(uint32_t *a,uint32_t *b,uint32_t *c){if(a)*a=spans;if(b)*b=samples;if(c)*c=fallbacks;}

/* Captured at the FX pull boundary, after all envelope sends have accumulated.
 * Failure retains the original CPU effect for that entire subframe. */
int native_dsp_mix_capture_wet(const int16_t *left,const int16_t *right,unsigned frames){
 if(!active || !external_fx || !current || !left || !right || !frames ||
    current->offset>current->frames || frames>current->frames-current->offset ||
    current->offset<current->wet_written)return 0;
 if(!current->wet_written)memset(current->wet_pcm,0,current->frames*4);
 for(unsigned i=0;i<frames;i++){
  current->wet_pcm[(current->offset+i)*2]=left[i];current->wet_pcm[(current->offset+i)*2+1]=right[i];
 }
 current->wet_written=current->offset+frames;return 1;
}
