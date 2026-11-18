#ifndef EOG_NATIVE_ROOM_RENDER_H
#define EOG_NATIVE_ROOM_RENDER_H
#include "system.h"
/* Counts the engine's actual primary-room decisions, independently of GPU
 * triangle culling. No extra per-room or per-frame log writes. */
static struct NativeRoomRenderStats {
    unsigned drawn,loaded,unloaded,budget_bypass,missing,distance_culled,frames;
} native_room_render_stats;
static void native_room_render_reset(void) {
    native_room_render_stats=(struct NativeRoomRenderStats){0};
}
static void native_room_render_sample(void) {
    if(++native_room_render_stats.frames%120)return;
    sysLogPrintf(LOG_NOTE,"Room streaming: drawn=%u loaded=%u unloaded=%u budgetBypass=%u missing=%u fogCulled=%u",
      native_room_render_stats.drawn,native_room_render_stats.loaded,native_room_render_stats.unloaded,
      native_room_render_stats.budget_bypass,native_room_render_stats.missing,native_room_render_stats.distance_culled);
    native_room_render_reset();
}
#endif
