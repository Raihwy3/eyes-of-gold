/* Interpreter entry adapter; game/GBI headers stay out of the GPU backend. */
#include "gfx_pc.h"
#include "native_graphics.h"
extern GfxRenderingAPI native_pica_api;
extern int gfx_fog_vertex;
extern GfxWindowManagerAPI native_window_api;
extern "C" unsigned native_graphics_frames(void);
extern "C" unsigned videoGetFrameCount(void) {
  return native_graphics_frames();
}
extern "C" void native_graphics_set_native_resolution(int w,int h) {
  if(w<=0 || h<=0 || w>2048 || h>2048)
    sysFatalError("Invalid original VI resolution %dx%d",w,h);
  gfx_current_native_viewport={0,0,(unsigned)w,(unsigned)h};
  gfx_current_native_aspect=(float)w/h;
}
extern "C" int native_graphics_init(void) {
  GfxInitSettings settings = {};
  settings.rapi = &native_pica_api;
  settings.wapi = &native_window_api;
  settings.window_settings.width = 400;
  settings.window_settings.height = 240;
  settings.window_settings.title = "Eyes of Gold: Native graphics";
  native_graphics_set_native_resolution(440,330);
  gfx_framebuffers_enabled = false;
  gfx_msaa_level = 1;
  gfx_init(&settings);
  gfx_fog_vertex = 1; /* Original RSP vertex fog; PICA interpolates its ramp. */
  gfx_set_output_aspect(4.f / 3.f);
  gfx_set_safe_area_crop(0);
  gfx_set_texture_filter(FILTER_NONE);
  gfx_set_mipmap_filter(MIPMAP_DISABLED);
  return 1;
}
extern "C" void native_graphics_render(void *commands) {
  gfx_start_frame();
  gfx_run(static_cast<Gfx *>(commands));
  gfx_end_frame();
}
extern "C" void native_graphics_destroy_interpreter(void) { gfx_destroy(); }
