"""Route original per-voice dry PCM to NDSP while retaining wet/reverb state."""
def mixer(text):
    text='#include "native_dsp_mix.h"\n#include "synthInternals.h"\n'+text
    needle='    for (i = 0; i < nsamples; i++) {'
    assert text.count(needle)==1
    text=text.replace(needle,'''    s16 *native_dry=NULL;
    if(sCtx.out>=AL_MAIN_L_OUT && !(sCtx.out&1))
        native_dry=native_dsp_mix_target((uintptr_t)saved,(sCtx.out-AL_MAIN_L_OUT)/2,nsamples);
'''+needle)
    old='''        dry[0][i] = mixerClamp16(dry[0][i] + (((s32)insamp * gain[0]) >> 15));
        dry[1][i] = mixerClamp16(dry[1][i] + (((s32)insamp * gain[1]) >> 15));'''
    assert text.count(old)==1
    text=text.replace(old,'''        if(native_dry) {
            native_dry[i*2]=mixerClamp16(((s32)insamp*gain[0])>>15);
            native_dry[i*2+1]=mixerClamp16(((s32)insamp*gain[1])>>15);
        }else {
'''+old+'''
        }''')
    return text

def synth(text):
    text='#include "native_dsp_mix.h"\n'+text
    needle='        outLen -= nOut;'
    assert text.count(needle)==1
    text=text.replace(needle,'        native_dsp_mix_advance(nOut);\n'+needle)
    return text
