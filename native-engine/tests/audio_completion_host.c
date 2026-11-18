/* Run the generated audio producer while the graphics scheduler is busy. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "native_dsp_mix.h"
typedef int16_t s16;typedef int32_t s32;typedef uint16_t u16;typedef uint32_t u32;
typedef uint64_t u64;typedef void *OSMesg;typedef uint64_t Acmd;
typedef struct {int mtqueue,queued;OSMesg message;} OSMesgQueue;
typedef struct {void *next;unsigned flags;OSMesgQueue *msgQ;OSMesg msg;
 struct {struct {u64 *data_ptr,*ucode_boot,*ucode,*ucode_data,*yield_data_ptr;
 unsigned data_size,type,ucode_boot_size,flags,ucode_data_size,yield_data_size;} t;} list;} OSScTask;
typedef struct {s16 *data,frameSamples;OSScTask task;} AudioInfo;
static struct {OSMesgQueue replyMessageQueue;Acmd *cmdList[2];} g_AudioManager;
static unsigned g_CurrentAcmdList,g_CommandLength,g_FrameSize=736,g_MinFrameSize=720;
static unsigned gsp3DTextStart[4],rspbootTextStart[4],aspMainTextStart[4],aspMainDataStart[4];
static unsigned synthesized,submitted,expected_submitted;static AudioInfo *expected_info;
static unsigned queued_bytes;static uint64_t clock_us;
#define EXTRA_SAMPLES 37
#define OS_SC_NEEDS_RSP 1
#define M_AUDTASK 2
#define SP_UCODE_DATA_SIZE 2048
#define OS_MESG_BLOCK 1
#define LOG_NOTE 2
static void amClearDmaBuffers(void){}
static uintptr_t osVirtualToPhysical(void *p){return (uintptr_t)p;}
static void osAiSetNextBuffer(const s16 *p,unsigned bytes){assert(p && bytes>=720*4 && bytes<=784*4);submitted++;}
static unsigned osAiGetLength(void){return queued_bytes;}
static uint64_t sysGetMicroseconds(void){return ++clock_us;}
static void native_audio_pcm_stats(uint32_t *a,uint32_t *b){*a=*b=0;}
static void sysLogPrintf(int level,const char *fmt,...){(void)level;(void)fmt;}
static Acmd *alAudioFrame(Acmd *cmd,unsigned *length,s16 *out,unsigned frames){
 assert(out==expected_info->data && frames>=720 && frames<=784);
 assert(submitted==expected_submitted);assert(!g_AudioManager.replyMessageQueue.queued);
 assert(native_dsp_mix_target(123,0,frames));
 memset(out,0,frames*4);native_dsp_mix_advance(frames);synthesized++;*length=0;return cmd;
}
static int osSendMesg(OSMesgQueue *queue,OSMesg message,int flags){
 /* No scheduler command queue or fake SP completion may be touched. */
 assert(queue==&g_AudioManager.replyMessageQueue && message==expected_info && flags==OS_MESG_BLOCK);
 assert(synthesized && !queue->queued);
 assert(native_dsp_mix_peek(expected_info->data,expected_info->frameSamples));
 queue->message=message;queue->queued=1;return 0;
}
#include "native-audio-producer.h"
int main(void){
 Acmd commands[2][8];s16 pcm[3][1024*2];AudioInfo info[3]={0};
 for(unsigned i=0;i<3;i++)info[i].data=pcm[i];
 g_AudioManager.cmdList[0]=commands[0];g_AudioManager.cmdList[1]=commands[1];
 assert(native_dsp_mix_init(1));AudioInfo *last=NULL;
 for(unsigned frame=0;frame<900;frame++){
  expected_info=&info[frame%3];expected_submitted=frame;
  queued_bytes=(frame%3)*512; /* Exercise minimum and top-up sizes. */
  amHandleFrameMessage(expected_info,last);
  assert(g_AudioManager.replyMessageQueue.queued);
  assert(g_AudioManager.replyMessageQueue.message==expected_info);
  g_AudioManager.replyMessageQueue.queued=0;
  if(last)native_dsp_mix_release(last->data);
  last=expected_info;
 }
 assert(synthesized==900 && submitted==899 && !g_AudioManager.replyMessageQueue.queued);
 puts("PASS actual audio producer: 900 completed blocks without graphics scheduling; original three-buffer pipeline, frame sizing, sidecar sealing and exactly one reply per block");
}
