#include "native_dsp_mix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    int keys[4];assert(!native_dsp_mix_init(0));native_dsp_mix_begin(keys,128);assert(!native_dsp_mix_target(1,0,16));
    assert(native_dsp_mix_init(1));native_dsp_mix_begin(keys,128);
    for(unsigned voice=1;voice<=23;voice++) {
        int16_t *p=native_dsp_mix_target(voice,7,32);assert(p);for(unsigned i=0;i<64;i++)p[i]=(int16_t)voice;
    }
    assert(!native_dsp_mix_target(1,8,8)); /* An overlapping pull must add via CPU, never overwrite its earlier stem. */
    assert(!native_dsp_mix_target(24,0,32));assert(!native_dsp_mix_target(1,120,32));
    assert(!native_dsp_mix_peek(keys,128));native_dsp_mix_advance(64);
    int16_t *p=native_dsp_mix_target(1,0,64);assert(p);for(unsigned i=0;i<128;i++)p[i]=99;
    native_dsp_mix_advance(64);native_dsp_mix_end();
    const struct NativeDspMixJob *job=native_dsp_mix_peek(keys,128);assert(job && job->offset==128 && job->mask==0x7fffff);
    assert(job->pcm[0]==0 && job->pcm[14]==1 && job->pcm[128]==99 && job->pcm[255]==99);
    int16_t copy[256];memcpy(copy,job->pcm,sizeof copy);
    for(unsigned i=1;i<3;i++){native_dsp_mix_begin(keys+i,128);assert(native_dsp_mix_target(1,0,128));native_dsp_mix_advance(128);native_dsp_mix_end();}
    native_dsp_mix_begin(keys+3,128);assert(!native_dsp_mix_target(1,0,128));native_dsp_mix_end();assert(!memcmp(copy,job->pcm,sizeof copy));
    native_dsp_mix_release(keys);native_dsp_mix_begin(keys+3,1024);assert(native_dsp_mix_target(1,1023,1));assert(!native_dsp_mix_target(1,1023,2));native_dsp_mix_advance(1024);native_dsp_mix_end();
    assert(!native_dsp_mix_peek(keys+3,128) && native_dsp_mix_peek(keys+3,1024));
    uint32_t a,b,c;native_dsp_mix_stats(&a,&b,&c);assert(a>23 && b>1000 && c>=2);
    puts("PASS DSP voice sidecars: exact subframe offsets, partial voices/silence, 24th-voice CPU overflow, three-job lifetime and buffer bounds");
}
