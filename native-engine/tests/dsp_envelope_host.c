#include "native_dsp_mix.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int16_t s16;typedef int32_t s32;typedef uint32_t u32;typedef uint64_t u64;
#define A_INIT 1
#define AL_MAIN_L_OUT 1088
#define GE_ENVFLAG(x) 0
#define MTRACE(...) ((void)0)
static uint64_t dmem[512];
#define DMEM_S16(a) ((s16 *)((unsigned char *)dmem+(a)))
static struct {unsigned in,out,count,dryR,wetL,wetR;} sCtx;
static struct {s16 volCur[2],volTgt[2];s32 volRate[2];s16 dryamt,wetamt;} sVol;
static FILE *s_voiceDumpFile;
static void *current_state;
static void *osPhysicalToVirtual(u32 address){(void)address;return current_state;}
static u64 sysGetMicroseconds(void){return 0;}
static s16 mixerClamp16(s32 x){return x<-32768?-32768:x>32767?32767:x;}
#include "native-env-reference.h"
#include "native-env-dsp.h"
struct State {s32 t[2],rate[2];s16 tgt[2],dry,wet;};
static struct State reference_states[3],dsp_states[3];
static int16_t reference_dry[128*2],base_dry[128*2],reference_wet[64*2];
static void setup(unsigned voice,unsigned local) {
    sCtx.in=0;sCtx.out=1088+local*2;sCtx.dryR=1408+local*2;sCtx.wetL=1728+local*2;sCtx.wetR=2048+local*2;sCtx.count=(64-local)*2;
    for(unsigned i=0;i<64-local;i++)DMEM_S16(0)[i]=(s16)((int)((i*139+voice*317)%5000)-2500);
    sVol.volCur[0]=12000+voice*1300;sVol.volCur[1]=14000-voice*900;
    sVol.volTgt[0]=18000-voice*1500;sVol.volTgt[1]=8000+voice*2000;
    sVol.volRate[0]=8*65536;sVol.volRate[1]=-8*65536;
    sVol.dryamt=23000;sVol.wetamt=voice?17000:0;
}
int main(void) {
    assert(native_dsp_mix_init(1));int key;
    for(unsigned pass=0;pass<12;pass++) {
        memset(reference_states,0,sizeof reference_states);memset(dsp_states,0,sizeof dsp_states);
        native_dsp_mix_begin(&key,128);
        // Keep the two real dry voices and fill the other hardware channels;
        // the third real voice exercises original CPU overflow summation.
        assert(native_dsp_mix_target((uintptr_t)&dsp_states[0],0,1));assert(native_dsp_mix_target((uintptr_t)&dsp_states[1],0,1));
        for(unsigned i=0;i<21;i++)assert(native_dsp_mix_target(1000+i,0,1));
        for(unsigned chunk=0;chunk<2;chunk++) {
            unsigned local=chunk?0:3;
            memset(dmem,0,sizeof dmem);
            for(unsigned voice=0;voice<3;voice++){setup(voice,local);sVol.volCur[0]+=pass*10;current_state=&reference_states[voice];original_env(chunk?0:1,1);}
            for(unsigned i=0;i<64;i++) {
                reference_dry[(chunk*64+i)*2]=DMEM_S16(1088)[i];reference_dry[(chunk*64+i)*2+1]=DMEM_S16(1408)[i];
                reference_wet[i*2]=DMEM_S16(1728)[i];reference_wet[i*2+1]=DMEM_S16(2048)[i];
            }
            memset(dmem,0,sizeof dmem);
            for(unsigned voice=0;voice<3;voice++){setup(voice,local);sVol.volCur[0]+=pass*10;current_state=&dsp_states[voice];aEnvMixerImpl(chunk?0:1,1);}
            for(unsigned i=0;i<64;i++) {
                base_dry[(chunk*64+i)*2]=DMEM_S16(1088)[i];base_dry[(chunk*64+i)*2+1]=DMEM_S16(1408)[i];
                assert(DMEM_S16(1728)[i]==reference_wet[i*2] && DMEM_S16(2048)[i]==reference_wet[i*2+1]);
            }
            assert(!memcmp(reference_states,dsp_states,sizeof dsp_states));native_dsp_mix_advance(64);
        }
        native_dsp_mix_end();const struct NativeDspMixJob *job=native_dsp_mix_peek(&key,128);assert(job);
        for(unsigned i=0;i<256;i++)assert((int)base_dry[i]+job->pcm[i]+job->pcm[NATIVE_DSP_MIX_FRAMES*2+i]==reference_dry[i]);
        native_dsp_mix_release(&key);
    }
    puts("PASS actual DSP envelope/reference: isolated dry voices reconstruct original unsaturated sum; wet sends and saved envelope state remain byte-exact across INIT/CONTINUE, ramps, subframes and CPU overflow");
}
