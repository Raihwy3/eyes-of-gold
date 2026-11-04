/* Console host for the original game loop. No recreation gameplay. */
#include "native_build.h"
#include "native_version.h"
#include "native_boot_progress.h"
#include "native_game.h"
#include "native_audio.h"
#include "native_graphics.h"
#include "native_input.h"
#include "native_console.h"
#include "native_bottom.h"
#include "native_log_buffer.h"
#include "native_log_policy.h"
#include "native_work_profile.h"
#include <3ds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(CFG_MODEL_N3DS==2 && CFG_MODEL_N3DSXL==4 && CFG_MODEL_N2DSXL==5,
               "Console model identifiers changed");
/* First-launch mesh reduction nests larger bounded workspaces than gameplay. */
u32 __stacksize__=128*1024;
static int console_model=-1;
int native_platform_console_model(void){return console_model;}
static void detect_console_model(void) {
  Result result=cfguInit();
  if(R_SUCCEEDED(result)) {
    u8 model;
    if(R_SUCCEEDED(CFGU_GetSystemModel(&model)))console_model=model;
    cfguExit();
  }
  if(console_model<0 || console_model>5) {
    bool newer=false;
    if(R_SUCCEEDED(APT_CheckNew3DS(&newer)))console_model=newer?CFG_MODEL_N3DS:CFG_MODEL_3DS;
  }
}
void sysLogPrintf(int level,const char *fmt,...);
extern void native_derived_memory_init(void);
static PrintConsole top, bottom;
static aptHookCookie lifecycle_cookie;
static void lifecycle_event(APT_HookType event,void *unused) {
  (void)unused;
  if(event==APTHOOK_ONEXIT)native_game_request_quit();
}
extern unsigned native_graphics_phase(void);
static FILE *log_file;
static struct NativeLogBuffer log_queue=NATIVE_LOG_INITIALIZER;
static int log_worker_started;
static volatile int log_worker_stop,log_worker_exited;
static __thread int engine_worker;
void native_platform_log_batch_begin(void){}
void native_platform_log_batch_end(void){}
static void native_log_worker(void *unused) {
  (void)unused;
  while(!__atomic_load_n(&log_worker_stop,__ATOMIC_ACQUIRE)) {native_log_drain(&log_queue,log_file,0);svcSleepThread(250000000);}
  __atomic_store_n(&log_worker_exited,1,__ATOMIC_RELEASE);
}
/* libctru's pthread implementation also uses threadCreate (core 0). Select
 * the processor at creation: SetThreadAffinityMask is stubbed by modern FIRM.
 * This uses the same libctru TLS/newlib setup and original worker entry/queue. */
static struct NativeRenderStart {
  void *(*entry)(void *);
  void *arg;
} native_render_start;
static void native_render_entry(void *arg) {
  struct NativeRenderStart *start=arg;
  start->entry(start->arg);
}
int native_platform_start_render(void *(*entry)(void *),void *arg,size_t stack_size) {
  if(!native_console_is_new(console_model))return 0;
  native_render_start.entry=entry;native_render_start.arg=arg;
  Thread worker=threadCreate(native_render_entry,&native_render_start,stack_size,0x3f,2,true);
  sysLogPrintf(2,"Render worker creation: requested core 2, %s",
    worker?"created; entry will report actual core":"unavailable; falling back to original pthread/core 0");
  return worker!=NULL;
}
/* Keep the original audio manager and its locks/queues intact. It owns the
 * CPU synthesizer, PCM voice processing, envelopes and reverb; NDSP continues
 * to sum dry voices independently. New models put this worker on core 0 so
 * launchers that redirect syscore requests to core 2 cannot crowd rendering.
 * Old models retain the existing syscore allowance/fallback policy. */
static struct NativeAudioStart {
  void *(*entry)(void *);
  void *arg;
  int requested_core;
} native_audio_start;
static void native_audio_entry(void *arg) {
  struct NativeAudioStart *start=arg;
  sysLogPrintf(2,"Audio CPU worker: core=%ld (requested core %d)",(long)svcGetProcessorID(),start->requested_core);
  start->entry(start->arg);
}
int native_platform_start_audio(void *(*entry)(void *),void *arg,size_t stack_size) {
  const int requested_core=native_console_is_new(console_model)?0:1;
  if(requested_core==1) {
    /* CFW/launcher permissions differ. Ask for headroom; an idle audio worker
     * sleeps on its existing queues rather than consuming the whole allowance. */
    Result result=APT_SetAppCpuTimeLimit(89);
    unsigned requested=89;
    if(R_FAILED(result)){requested=50;result=APT_SetAppCpuTimeLimit(requested);}
    if(R_FAILED(result)){requested=30;result=APT_SetAppCpuTimeLimit(requested);}
    if(R_FAILED(result)) {
      sysLogPrintf(2,"Audio core 1 unavailable: APT CPU allowance failed %08lx; using core 0",(unsigned long)result);
      return 0;
    }
    u32 reported=0;
    Result query=APT_GetAppCpuTimeLimit(&reported);
    sysLogPrintf(2,"Audio core 1 allowance: requested=%u%% reported=%lu%% query=%08lx (allowance, not measured CPU use)",requested,(unsigned long)reported,(unsigned long)query);
  }
  native_audio_start.entry=entry;native_audio_start.arg=arg;native_audio_start.requested_core=requested_core;
  Thread worker=threadCreate(native_audio_entry,&native_audio_start,stack_size,0x3f,requested_core,true);
  sysLogPrintf(2,"Audio worker creation: requested core %d, %s",requested_core,worker?"created; entry will report actual core":"unavailable; using original pthread/core 0");
  return worker!=NULL;
}
void native_platform_worker_info(const char *name) {
  engine_worker=1;
  u8 affinity[16]={0};s32 ideal=-1,priority=-1;
  Result result=svcGetThreadAffinityMask(affinity,CUR_THREAD_HANDLE,native_console_is_new(console_model)?4:2);
  svcGetThreadIdealProcessor(&ideal,CUR_THREAD_HANDLE);
  svcGetThreadPriority(&priority,CUR_THREAD_HANDLE);
  sysLogPrintf(2,"Worker %s: core=%ld affinity=%02x,%02x,%02x,%02x ideal=%ld priority=%ld query=%08lx",name,(long)svcGetProcessorID(),affinity[0],affinity[1],affinity[2],affinity[3],(long)ideal,(long)priority,(unsigned long)result);
}
static volatile int running, failed, fatal_claimed;
static char fatal_message[768];
static __thread unsigned char exception_stack[4096] __attribute__((aligned(8)));
static __thread ERRF_ExceptionData exception_data;
static ERRF_ExceptionData captured_exception;
static volatile int exception_pending;
/* Keep the faulting thread parked. The console thread writes the report so the
 * exception handler never calls stdio, allocators, or normal application locks. */
static void __attribute__((noreturn, target("arm")))
capture_exception(ERRF_ExceptionInfo *info, CpuRegisters *regs) {
  if(__sync_lock_test_and_set(&fatal_claimed,1))for(;;)svcSleepThread(100000000);
  captured_exception.excep = *info;
  captured_exception.regs = *regs;
  __sync_synchronize();
  exception_pending = 1;
  for (;;) svcSleepThread(100000000);
}
void native_platform_started(void) { running = 1; }
void native_platform_crash_init(void) {
  threadOnException(capture_exception,
                    exception_stack + sizeof exception_stack, &exception_data);
}
const char *sysResolvePath(const char *path) {
  static __thread char output[512];
  if (path[0] == '$' && (path[1] == 'S' || path[1] == 'E') && path[2] == '/')
    path += 3;
  snprintf(output, sizeof output, "sdmc:/3ds/eyes-of-gold-native-game/%s",
           path);
  return output;
}
static void log_args(const char *fmt,va_list args,int newline) {
  if(!log_file)return;
  char text[2048];
  int n=vsnprintf(text,sizeof text-2,fmt,args);
  if(n<0)return;
  if(n>(int)sizeof text-3)n=sizeof text-3;
  if(newline)text[n++]='\n';
  native_log_enqueue(&log_queue,text,(unsigned)n);
}
void osSyncPrintf(const char *fmt,...) {
  va_list args;va_start(args,fmt);log_args(fmt,args,0);va_end(args);
}
void sysLogPrintf(int level,const char *fmt,...) {
  if(!log_file)return;
  // Fatal/error diagnostics retain synchronous durability. Ordinary game,
  // renderer and mixer messages never write to SD from their worker thread.
  if(level==0)native_log_drain(&log_queue,log_file,1);
  va_list args;va_start(args,fmt);log_args(fmt,args,1);va_end(args);
  if(level==0)native_log_drain(&log_queue,log_file,1);
}
u64 sysGetMicroseconds(void) {
  u64 t = svcGetSystemTick();
  return t / SYSCLOCK_ARM11 * 1000000u +
         t % SYSCLOCK_ARM11 * 1000000u / SYSCLOCK_ARM11;
}
/* Read the flag once at boot via native_log_open; no SD access on these paths. */
uint64_t native_work_profile_clock(void) {return log_file?sysGetMicroseconds():0;}
void native_work_profile_record(unsigned stage,uint64_t begin,uint64_t end) {
  static __thread struct NativeWorkWindow windows[NATIVE_WORK_STAGE_COUNT];
  static __thread int measured_core=-1;
  static const char *const names[]={"main-work","main-wait-gfx","main-wait-vi",
    "scene-build","render-work","render-idle","fps-wait","gpu-wait","gpu-submit","stage-load"};
  if(!log_file || stage>=NATIVE_WORK_STAGE_COUNT)return;
  if(measured_core<0)measured_core=svcGetProcessorID();
  struct NativeWorkWindow *w=&windows[stage];
  int ready=native_work_window_add(w,begin,end);
  if(!w->count || (!ready && stage!=NATIVE_WORK_STAGE_LOAD))return;
  double span=(double)(end-w->begin);
  sysLogPrintf(2,"Worker timing: stage=%s core=%d windowMs=%.2f samples=%u avgMs=%.3f peakMs=%.3f spanShare=%.1f%% (wall time, nested rows; not CPU utilization)",
    names[stage],measured_core,span/1000.,w->count,(double)w->total/w->count/1000.,
    (double)w->peak/1000.,span>0?100.*(double)w->total/span:0.);
  memset(w,0,sizeof *w);
}

void sysSleep(u32 us) {
  if(engine_worker && videoQuitRequested())threadExit(0);
  svcSleepThread((s64)us * 1000);
}
uintptr_t sysImageBase(void) {
  extern char _start[];
  return (uintptr_t)_start;
}
void sysFatalError(const char *fmt, ...) {
  if(__sync_lock_test_and_set(&fatal_claimed,1))for(;;)svcSleepThread(100000000);
  va_list a;
  va_start(a, fmt);
  vsnprintf(fatal_message, sizeof fatal_message, fmt, a);
  va_end(a);
  sysLogPrintf(0, "FATAL: %s", fatal_message);
  __sync_synchronize();
  failed = 1;
  if (running)
    for (;;)
      svcSleepThread(100000000);
  consoleSelect(&bottom);
  consoleClear();
  printf("Native game startup failed\n\n%s\n\nSTART: exit\n", fatal_message);
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START)
      break;
    gfxFlushBuffers();
    gspWaitForVBlank();
  }
  /* Startup failures must stop GSP before the SDK releases the heap holding
   * its event-thread stack. exit() alone leaves that thread running. */
  gfxExit();
  __atomic_store_n(&log_worker_stop,1,__ATOMIC_RELEASE);
  for(unsigned i=0;log_worker_started && !__atomic_load_n(&log_worker_exited,__ATOMIC_ACQUIRE) && i<500;i++)svcSleepThread(1000000);
  exit(1);
}
/* Text-mode startup stays available before assets and the renderer exist.
 * Both screens use only original launcher text and the SDK console font. */
static void boot_screen(const char *stage,unsigned done,unsigned total,int converting) {
  static unsigned overall;
  if(converting)overall=native_boot_total(stage,done,total,overall);
  const unsigned percent=total?(unsigned)((u64)done*100/total):0;
  const unsigned bounded=percent>100?100:percent;
  char bar[33];
  for(unsigned i=0;i<32;i++)bar[i]=i<bounded*32/100?'=':'-';
  bar[32]=0;
  consoleSelect(&top);
  consoleClear();
  printf("\x1b[33m"
         "\x1b[3;5H+----------------------------------------+"
         "\x1b[4;5H|             FIELD OPERATIONS           |"
         "\x1b[5;5H+----------------------------------------+"
         "\x1b[8;23H  |  "
         "\x1b[9;23H--O--"
         "\x1b[10;23H  |  "
         "\x1b[12;14HE Y E S   O F   G O L D"
         "\x1b[0m\x1b[14;16HNATIVE GAME LAUNCHER"
         "\x1b[18;9H%-34.34s",stage);
  if(converting && total)
    printf("\x1b[20;9H\x1b[33m[%s]\x1b[0m\x1b[22;23H%3u%%",bar,bounded);
  else if(converting)
    printf("\x1b[20;14HPreparing audio: %u frames",done);
  else
    printf("\x1b[20;14HPreparing your mission...");
  char total_bar[41];
  for(unsigned i=0;i<40;i++)total_bar[i]=i<overall*40/100?'=':'-';
  total_bar[40]=0;
  printf("\x1b[24;5HTOTAL PREPARATION %3u%%"
         "\x1b[26;5H\x1b[33m[%s]"
         "\x1b[0m\x1b[28;5HBuild " NATIVE_BUILD_ID,overall,total_bar);
  consoleSelect(&bottom);
  consoleClear();
  printf("\x1b[33m\x1b[3;4HMISSION PREPARATION"
         "\x1b[5;4H--------------------------------"
         "\x1b[0m\x1b[8;4HCONSOLE"
         "\x1b[10;4H%-32.32s"
         "\x1b[13;4H%s"
         "\x1b[16;4H%s"
         "\x1b[17;4H%s"
         "\x1b[21;4H%s"
         "\x1b[33m\x1b[24;4H--------------------------------"
         "\x1b[0m\x1b[26;4HEyes of Gold " NATIVE_ENGINE_VERSION,
         native_console_name(console_model),
         converting?"PREPARING GAME FILES":"CHECKING GAME FILES",
         converting?"New files are built from your":"Your saved settings will load",
         converting?"ROM only when needed.":"automatically.",
         converting?"B: cancel preparation":"Please wait...");
  if(converting==2) {
    consoleClear();
    printf("\x1b[33m\x1b[3;4HEYES OF GOLD"
           "\x1b[5;4H--------------------------------"
           "\x1b[0m\x1b[10;4HMISSION READY"
           "\x1b[13;4HGame files are ready."
           "\x1b[16;4HStarting the game..."
           "\x1b[19;4HTouch menus will appear here."
           "\x1b[33m\x1b[24;4H--------------------------------"
           "\x1b[0m\x1b[26;4HEyes of Gold " NATIVE_ENGINE_VERSION);
  }
  if(converting==1)printf("\x1b[19;4HKeep the console powered on.");
}
void native_platform_prepared(void) {
  boot_screen("Mission ready",1,1,2);
  gfxFlushBuffers();
  gfxSwapBuffers();
  gspWaitForVBlank();
}
int native_platform_convert_progress(const char *stage,unsigned done,unsigned total) {
  static u64 next_update;
  static const char *last_stage;
  if(!last_stage || strcmp(last_stage,stage)) {
    sysLogPrintf(2,"Asset preparation: %s",stage);
    last_stage=stage;
    next_update=0;
  }
  if(!aptMainLoop())return 0;
  hidScanInput();
  if(hidKeysDown() & KEY_B)return 0;
  u64 now=osGetTime();
  if(now<next_update)return 1;
  next_update=now+200;
  boot_screen(stage,done,total,1);
  gfxFlushBuffers();
  gfxSwapBuffers();
  gspWaitForVBlank();
  return 1;
}
int main(void) {
  native_derived_memory_init();
  gfxInitDefault();
  osSetSpeedupEnable(true);
  detect_console_model();
  consoleInit(GFX_TOP, &top);
  consoleInit(GFX_BOTTOM, &bottom);

  extern int native_update_startup(const char *,PrintConsole *,PrintConsole *);
  if(!native_update_startup("sdmc:/3ds/eyes-of-gold-native-game",&top,&bottom)){gfxExit();return 0;}

  native_input_init();
  native_bottom_init();
  aptHook(&lifecycle_cookie,lifecycle_event,NULL);
  hidScanInput();
  log_file=native_log_open("sdmc:/3ds/eyes-of-gold-native-game");
  if(log_file)log_worker_started=threadCreate(native_log_worker,NULL,16384,0x3f,0,true)!=NULL;
  sysLogPrintf(2, "Original-game boot build " NATIVE_BUILD_ID);
  sysLogPrintf(2,"Routine logging: bounded memory queue, %s",log_worker_started?"background SD writer":"UI drain fallback");
  boot_screen("INITIALIZING",0,0,0);
  gfxFlushBuffers();
  gfxSwapBuffers();
  gspWaitForVBlank();
  if (!native_game_start("sdmc:/3ds/eyes-of-gold-native-game"))
    sysFatalError("Missing/invalid native-game.dat or native-cart.bin");
  running = 1;
  unsigned last_frames = 0;
  u64 pace_start=sysGetMicroseconds();
  unsigned pace_frames=native_graphics_frames();
  u64 next_status=pace_start+5000000;
  int error_shown = 0;
  u64 next_log_flush=pace_start+250000,progress_at=pace_start,next_stall=pace_start+2000000;
  unsigned progress_frames=pace_frames;
  while (aptMainLoop()) {
    if (exception_pending && !error_shown) {
      __sync_synchronize();
      const CpuRegisters *regs = &captured_exception.regs;
      const ERRF_ExceptionInfo *info = &captured_exception.excep;
      snprintf(fatal_message, sizeof fatal_message,
               "CPU exception %u\nPC: %08lx LR: %08lx\nFault address: %08lx",
               (unsigned)info->type, (unsigned long)regs->pc,
               (unsigned long)regs->lr, (unsigned long)info->far);
      sysLogPrintf(0, "CPU EXCEPTION: build=" NATIVE_BUILD_ID
                   " type=%u PC=%08lx LR=%08lx SP=%08lx CPSR=%08lx FAR=%08lx FSR=%08lx",
                   (unsigned)info->type, (unsigned long)regs->pc,
                   (unsigned long)regs->lr, (unsigned long)regs->sp,
                   (unsigned long)regs->cpsr, (unsigned long)info->far,
                   (unsigned long)info->fsr);
      for (unsigned reg = 0; reg < 13; ++reg)
        sysLogPrintf(0, "  r%u=%08lx", reg, (unsigned long)regs->r[reg]);
      failed = 1;
    }
    if (failed && !error_shown) {
      native_bottom_stop();
      consoleInit(GFX_BOTTOM,&bottom);
      native_input_suspend();
      __sync_synchronize();
      consoleSelect(&bottom);
      consoleClear();
      printf("Original engine stopped\nBuild: " NATIVE_BUILD_ID
             "\n\n%s\n\nSTART: exit\n\n%s\n",
             fatal_message,log_file?"Details: native-game.log":"Logging is off. Add debug-log.flag\nto record the next launch.");
      error_shown = 1;
    }
    if (error_shown) {
      hidScanInput();
      if (hidKeysDown() & KEY_START)
        break;
      gfxFlushBuffers();
    } else if (native_input_exit_requested() || videoQuitRequested())
      break;
    if(!error_shown)native_bottom_draw();
    unsigned frames = native_graphics_frames();
    u64 progress_now=sysGetMicroseconds();
    if(frames!=progress_frames){progress_frames=frames;progress_at=progress_now;next_stall=progress_now+2000000;}
    if(log_file && !failed && progress_now>=next_stall) {
      sysLogPrintf(2,"Frame stall: no presentation for %.2fs frames=%u GPUphase=%u renderIdle=%d",
        (double)(progress_now-progress_at)/1000000.0,frames,native_graphics_phase(),native_game_render_idle());
      next_stall=progress_now+5000000;
    }
    if(log_file && !log_worker_started && progress_now>=next_log_flush) {
      native_log_drain(&log_queue,log_file,0);next_log_flush=progress_now+250000;
    }
    if (frames / 120 != last_frames / 120) {
      sysLogPrintf(2, "Original game frames: %u", frames);
      last_frames = frames;
    }
    if(log_file && sysGetMicroseconds()>=next_status) {
      sysLogPrintf(2,"UI heartbeat: frames=%u GPUphase=%u renderIdle=%d failed=%d",
                   frames,native_graphics_phase(),native_game_render_idle(),failed);
      u64 now=sysGetMicroseconds();
      if(now>pace_start && !failed)
        sysLogPrintf(2,"Frame pace: submitted=%.2f FPS over %.2fs (%u frames; includes game/scheduling/wait)",
          (double)(frames-pace_frames)*1000000.0/(now-pace_start),
          (double)(now-pace_start)/1000000.0,frames-pace_frames);
      pace_start=now;pace_frames=frames;next_status=now+5000000;
    }
    /* Keep APT/HOME responsive when the renderer is waiting for GPU work. */
    svcSleepThread(16000000);
  }
  /* Use the normal SDK APT/loader return. A raw svcExitProcess bypasses
   * the homebrew return callback and can strand the host title on closing. */
  native_input_suspend();
  native_game_request_quit();
  sysLogPrintf(2,"Shutdown requested: GPUphase=%u",native_graphics_phase());
  svcSleepThread(16000000);
  for(unsigned i=0;i<120 && !native_game_render_idle();++i)svcSleepThread(1000000);
  int audio_stopped=native_audio_shutdown();
  sysLogPrintf(2,"Shutdown: audio stopped=%d renderIdle=%d",audio_stopped,native_game_render_idle());
  if(native_game_render_idle()) {
    sysLogPrintf(2,"Shutdown: releasing Citro3D");
    native_graphics_stop_presenting();
    sysLogPrintf(2,"Shutdown: releasing GSP display");
    gfxExit();
  }
  /* Close all remaining engine workers together with the title. */
  aptUnhook(&lifecycle_cookie);
  native_log_drain(&log_queue,log_file,1);
  sysLogPrintf(2,"Shutdown: returning through SDK APT/loader cleanup");
  __atomic_store_n(&log_worker_stop,1,__ATOMIC_RELEASE);
  for(unsigned i=0;log_worker_started && !__atomic_load_n(&log_worker_exited,__ATOMIC_ACQUIRE) && i<500;i++)svcSleepThread(1000000);
  native_log_drain(&log_queue,log_file,1);
  return 0;
}
