/* Original scheduler frame hooks -> the same real Fast3D/PICA backend. */
#include "gfx_pc.h"
#include "native_graphics.h"
#include "system.h"
#include "native_settings.h"
#include "native_frame_limit.h"
#include "native_dynamic_lod.h"
#include "native_work_profile.h"
static volatile int quit_requested, render_busy;
static uint64_t last_present_end,frame_render_start,frame_work_start;
static unsigned scene_work_us;
extern "C" unsigned native_game_render_state(void) __attribute__((weak));
extern "C" int getPlayerCount(void);
extern "C" float portNativeAspect(void);
extern "C" int get_currentmenu(void);
extern "C" void native_pica_frontend_ui(int);
extern "C" unsigned char gunbarrel_mode;
extern "C" int intro_eye_counter;
extern "C" float g_TitleX,g_TitleY;
extern "C" void native_guard_profile_report(void) __attribute__((weak));
extern "C" void native_guard_pose_report(void) __attribute__((weak));
extern "C" void native_effects_report(void) __attribute__((weak));
extern "C" void native_game_scene_built(unsigned long long us) {
  native_setting_set_render_state(native_game_render_state?native_game_render_state():0);
  __atomic_store_n(&scene_work_us,(unsigned)(us>2000000?2000000:us),__ATOMIC_RELAXED);
  static uint64_t total,peak;static unsigned count;
  total+=us;if(us>peak)peak=us;
  if(++count==120) {
    sysLogPrintf(LOG_NOTE,"Original scene build: avg=%.2fms peak=%.2fms samples=%u",double(total)/count/1000.,double(peak)/1000.,count);
    if(native_guard_pose_report)native_guard_pose_report();
    if(native_effects_report)native_effects_report();
    if(native_guard_profile_report)native_guard_profile_report();
    total=peak=0;count=0;
  }
}
extern "C" void videoStartFrame(void) {
  if (quit_requested)
    for (;;)
      sysSleep(100000);
  uint64_t arrival=sysGetMicroseconds();
  if(last_present_end && arrival-last_present_end>150000)
    sysLogPrintf(LOG_NOTE,"Frame build hitch: %.2fms menu=%d mode=%u",(double)(arrival-last_present_end)/1000.0,get_currentmenu(),gunbarrel_mode);
  frame_render_start=arrival;
  // Cap rendering, not the 60 Hz VI timebase: original simulation remains
  // driven by elapsed ticks at either rate.
  static NativeFrameLimit pacing;
  int limit=native_setting_fps();
  uint64_t wait=native_frame_wait(&pacing,sysGetMicroseconds(),limit);
  uint64_t native_pace_start=native_work_profile_begin();
  if(wait)sysSleep(wait);
  native_work_profile_end(NATIVE_WORK_FPS_WAIT,native_pace_start);
  native_frame_started(&pacing,sysGetMicroseconds(),limit);
  frame_work_start=sysGetMicroseconds();
  __atomic_store_n(&render_busy, 1, __ATOMIC_RELEASE);
  const float aspect=portNativeAspect();
  gfx_set_output_aspect(aspect>0?aspect:4.f/3.f);
  native_pica_frontend_ui(get_currentmenu()!=11);
  gfx_start_frame();
}
extern "C" void videoEndFrame(void) {
  static unsigned frame;
  static int last_menu=-999,last_mode=-999;
  const int menu=get_currentmenu();
  if(menu!=last_menu || gunbarrel_mode!=last_mode || frame%120==0) {
    sysLogPrintf(LOG_NOTE,"Intro state: frame=%u menu=%d mode=%d counter=%d xy=%.2f,%.2f",
                 frame,menu,gunbarrel_mode,intro_eye_counter,(double)g_TitleX,(double)g_TitleY);
    sysLogPrintf(LOG_NOTE,"Detail: lod=%d propDistance=%d (render only)",native_setting_lod(),native_setting_view_distance());
    last_menu=menu;last_mode=gunbarrel_mode;
  }
  ++frame;
  gfx_end_frame();
  uint64_t now=sysGetMicroseconds();
  if(now-frame_render_start>150000)
    sysLogPrintf(LOG_NOTE,"Frame render/wait hitch: %.2fms menu=%d mode=%u",(double)(now-frame_render_start)/1000.0,menu,gunbarrel_mode);
  static NativeDynamicLod dynamic;
  unsigned work=(unsigned)(now-frame_work_start)+__atomic_load_n(&scene_work_us,__ATOMIC_RELAXED);
  int strength=native_dynamic_lod_step(&dynamic,now,work,native_setting_fps(),native_setting_lod()==4,(native_setting_render_state()&1)!=0);
  native_setting_dynamic_lod(strength);
  last_present_end=now;
  __atomic_store_n(&render_busy, 0, __ATOMIC_RELEASE);
}
extern "C" void videoSyncSplitScreen(void) {
  if (getPlayerCount() > 2)
    sysFatalError("Native multiplayer preview supports two engine players");
}
extern "C" void videoUpdateNativeResolution(int w, int h) {
  native_graphics_set_native_resolution(w,h);
}
extern "C" int videoGetNativeWidth(void) {
  return gfx_current_native_viewport.width;
}
extern "C" int videoGetNativeHeight(void) {
  return gfx_current_native_viewport.height;
}
extern "C" void videoResetTextureCache(void) { gfx_texture_cache_clear(); }
extern "C" int videoQuitRequested(void) {
  return __atomic_load_n(&quit_requested, __ATOMIC_ACQUIRE);
}
extern "C" void native_game_request_quit(void) {
  __atomic_store_n(&quit_requested, 1, __ATOMIC_RELEASE);
}
extern "C" int native_game_render_idle(void) {
  return !__atomic_load_n(&render_busy, __ATOMIC_ACQUIRE);
}
