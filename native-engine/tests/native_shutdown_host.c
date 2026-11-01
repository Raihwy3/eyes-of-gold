/* Exercise the actual platform shutdown body and actual NDSP shutdown function. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "native_dsp_mix.h"
#define CHANNELS (NATIVE_DSP_DRY_CHANNELS+1)
static int idle,quit,suspended,presenting_stopped,gfx_stopped,unhooked,drains;
static int log_worker_started=1,log_worker_exited=1,log_worker_stop;
static int log_queue,log_file,lifecycle_cookie,sleeps;
static int ready,stopping,audio_lock,locked,cleared,ndsp_stopped,dsp_mode;
static void LightLock_Lock(int *p){assert(p==&audio_lock && !locked);locked=1;}
static void LightLock_Unlock(int *p){assert(p==&audio_lock && locked);locked=0;}
static void ndspChnWaveBufClear(int ch){assert(ch==cleared && locked && stopping);cleared++;}
static void ndspSetCallback(void (*cb)(void *),void *data){assert(!cb && !data && locked && stopping);}
static void ndspExit(void){assert(locked && stopping);ndsp_stopped++;}
#include "native-audio-shutdown.h"
static void native_input_suspend(void){suspended=1;}
static void native_game_request_quit(void){assert(suspended);quit=1;}
static unsigned native_graphics_phase(void){return idle?0:2;}
static int native_game_render_idle(void){return idle;}
static void sysLogPrintf(int level,const char *fmt,...){(void)level;(void)fmt;}
static void svcSleepThread(int64_t ns){assert(ns>0);sleeps++;}
static void native_graphics_stop_presenting(void){assert(quit && idle && !ready);presenting_stopped++;}
static void gfxExit(void){assert(presenting_stopped);gfx_stopped++;}
static void aptUnhook(int *p){assert(p==&lifecycle_cookie);unhooked++;}
static void native_log_drain(int *q,int f,int force){assert(q==&log_queue && f==log_file && force);drains++;}
static int actual_shutdown(void) {
#include "native-shutdown-body.h"
}
int main(void) {
 for(int mode=0;mode<2;mode++)for(int busy=0;busy<=1;busy++) {
  dsp_mode=mode;
  idle=!busy;quit=suspended=presenting_stopped=gfx_stopped=unhooked=drains=sleeps=0;
  stopping=log_worker_stop=cleared=ndsp_stopped=0;ready=1;
  assert(actual_shutdown()==0);
  assert(quit && suspended && !ready && stopping && !locked && cleared==(mode?CHANNELS:1) && ndsp_stopped==1);
  assert(presenting_stopped==!busy && gfx_stopped==!busy && unhooked==1 && drains>=1 && log_worker_stop);
  assert(sleeps<=121);
 }
 ready=0;cleared=ndsp_stopped=0;assert(native_audio_shutdown()==1 && !cleared && !ndsp_stopped && !locked);
 puts("PASS SDK shutdown return: bounded render wait, idle-only GPU teardown, audio closes without producer acknowledgement, log stop, APT hook release and normal main return");
}
