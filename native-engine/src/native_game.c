#include "native_audio_pcm.h"
#include "native_convert.h"
#include "native_rom_data.h"
#include "native_derived.h"
extern void native_platform_prepared(void) __attribute__((weak));
extern int native_platform_convert_progress(const char *,unsigned,unsigned);
/* Direct original boss/scheduler entry, separate from the graphics harness. */
// clang-format off
#include <ultra64.h>
#include "native_game.h"
#include "native_audio.h"
#include "boss.h"
#include "native_assets.h"
#include "native_graphics.h"
#include "native_ram.h"
#include "native_settings.h"
#include "native_console.h"
#include "native_render_reference.h"
#include "system.h"
#include "thread_config.h"
#include <memp.h>
// clang-format on
extern void schedulerInitThread(void);
extern OSThread mainThread;
extern void tokenSetString(const char *);
extern void romCreateMesgQueue(void);
extern s8 g_sndBootswitchSound;
extern void native_platform_crash_init(void);
extern void native_platform_started(void);
extern void native_muted_audio_init(void);
extern void mixerInit(void);
static int kernel_ready;
static const char *boot_tokens="-s";
static void game_entry(void *unused) {
  (void)unused;
  /* Direct boss entry bypasses the N64 bootstrap's crash initialization.
   * Exception handlers are thread-local, so install it on this worker. */
  native_platform_crash_init();
  romCreateMesgQueue();
  if (native_audio_ready()) boot_tokens="";
  tokenSetString(boot_tokens);
  g_sndBootswitchSound = !native_audio_ready();
  if (g_sndBootswitchSound) native_muted_audio_init();
  mixerInit();
  schedulerInitThread();
  sysLogPrintf(2, "Starting original bossEntry: %s, audio %s",boot_tokens,g_sndBootswitchSound?"disabled":"enabled");
  bossEntry();
  sysFatalError("Original bossEntry unexpectedly returned");
}
int native_game_start(const char *directory) {
  native_settings_init(directory);
  int reference_background=native_reference_background_enabled(directory);
  native_graphics_reference_background(reference_background);
  sysLogPrintf(2,"Background geometry: %s",reference_background?"CPU transforms (default)":"experimental GPU transforms (gpu-background.flag)");
  const int model=native_platform_console_model();
  const int defaults=native_settings_apply_console_defaults(model);
  sysLogPrintf(2,"Console: %s (model=%d)",native_console_name(model),model);
  sysLogPrintf(2,"Native options: %s; style=%d aspect=%d",
      defaults?"first-launch defaults applied":"existing configuration preserved",
      native_setting_style(),native_setting_aspect());
  if (!native_convert_ensure(directory,native_platform_convert_progress)) {
    sysFatalError("%s",native_convert_error());
    return 0;
  }
  if (!native_rom_data_init(directory)) {
    sysFatalError("%s",native_rom_data_error());
    return 0;
  }
  sysLogPrintf(2,"Original ROM data: %u definitions loaded",native_rom_data_count());
  if(!native_derived_mount(directory)) {
    sysFatalError("Unable to load validated native-render.dat");
    return 0;
  }
  if (!native_ram_init() || !native_assets_mount(directory))
    return 0;
  if(native_platform_prepared)native_platform_prepared();
  if (!native_graphics_init())
    return 0;
  const int pcm_ready=native_audio_pcm_mount(directory);
  sysLogPrintf(2,"Preprocessed audio samples: %s",pcm_ready?"PCM cache active (both banks)":"unavailable; original decode fallback");
  native_audio_configure(directory,native_setting_audio_mixer());
  native_audio_init();
  portKernelInit();
  kernel_ready = 1;
  osCreateThread(&mainThread, MAIN_THREAD_ID, game_entry, NULL, NULL,
                 MAIN_THREAD_PRIORITY);
  native_platform_started();
  osStartThread(&mainThread);
  return 1;
}
const char *sysGetTokenString(void) { return boot_tokens; }
s32 rmonGetToken(void) { return 0; } /* No Indy debugger attached. */
void osReadHost(void *buffer, u32 bytes) {
  (void)buffer;
  (void)bytes;
  sysFatalError("Indy host reads are unavailable on 3DS");
}
void osWriteHost(void *buffer, u32 bytes) {
  (void)buffer;
  (void)bytes;
  sysFatalError("Indy host writes are unavailable on 3DS");
}
void tlbmanageEstablishManagementTable(void) {
  if (!native_ram)
    sysFatalError("Native game RAM has not been allocated");
  /* Cart streams are served by native_cart; no N64 TLB mapping is installed. */
  sysLogPrintf(
      2, "RAM translation: native allocated arena, file-backed cartridge");
}
void tlbmanageResetCurrentEntriesCount(void) {
  if (!native_ram)
    sysFatalError("Cannot reset mappings before native RAM exists");
}
void osCreateViManager(OSPri priority) {
  (void)priority;
  if (!kernel_ready)
    sysFatalError("VI retrace source requires the native thread kernel");
  /* portKernelInit owns the real retrace/event thread used by sched.c. */
}
void crashInit(void) { native_platform_crash_init(); }
void crashRenderFrame(u16 *buffer) {
  (void)buffer;
  sysFatalError(
      "Original engine requested its debug crash screen");
}
void native_audio_bank_fixup(void *blob,uint32_t bytes,uint32_t table) {
  if (!native_audio_bank_convert(blob,bytes) || !native_audio_register_raw(blob,table)) sysFatalError("Invalid native audio bank (%u bytes)",bytes);
}
void crashDumpThreads(const unsigned long *ids, const char **names, int count) {
  sysLogPrintf(0, "Native watchdog thread list (live foreign-thread unwinding "
                  "unavailable):");
  for (int i = 0; i < count; i++)
    sysLogPrintf(0, "  %s: %lu", names[i], ids[i]);
}
