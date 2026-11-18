#include <ultra64.h>
#include "bondtypes.h"
#include "bg.h"
#include "native_room_render.h"
#include "native_scenery_scope.h"
#include "native_room_packets.h"
#include "native_baked_lod.h"
#include "native_test_assert.h"
#include <stdio.h>
#include <string.h>
s_room_info g_BgRoomInfo[16];
int g_MaxNumRooms=16;
static int D_8004485C=1,D_80044858,g_RoomLoadBudget;
static int loads,fail_room,hidden_room;
static Vtx vertices[3];static Gfx room_commands[2];
uintptr_t native_ram_translate_kseg(uintptr_t address){return address;}
void sysLogPrintf(enum LogLevel level,const char *format,...){(void)level;(void)format;}
void osSyncPrintf(const char *format,...){(void)format;}
static int native_distance_room_visible(int room){return room!=hidden_room;}
static unsigned native_room_detail_tier(int room){(void)room;return 0;}
static Gfx *applyRoomMatrixToDisplayList(Gfx *gdl,int room){(void)room;return gdl;}
static void bgLoadRoomModelData(int room) {
    loads++;if(room==fail_room)return;
    g_BgRoomInfo[room].model_bin_loaded=1;
    g_BgRoomInfo[room].vertices=vertices;
    g_BgRoomInfo[room].ptr_expanded_mapping_info=room_commands;
}
#include "native-room-render-body.h"
int main(void) {
    Gfx commands[128],*p=commands;g_RoomLoadBudget=3;
    for(int room=1;room<=4;room++)p=bgRenderRoomPrimary(p,room);
    assert(loads==4 && native_room_render_stats.drawn==4);
    assert(g_RoomLoadBudget==0 && native_room_render_stats.budget_bypass==1 && !native_room_render_stats.missing);
    int old_loads=loads;Gfx *old=p;
    for(int room=1;room<=4;room++)p=bgRenderRoomPrimary(p,room);
    assert(loads==old_loads && native_room_render_stats.drawn==8 && p>old);
    fail_room=5;old=p;p=bgRenderRoomPrimary(p,5);
    assert(p==old && native_room_render_stats.missing==1 && loads==5);
    hidden_room=6;p=bgRenderRoomPrimary(p,6);
    assert(p==old && loads==5 && native_room_render_stats.distance_culled==1);
    p=bgRenderRoomPrimary(p,16);assert(p==old && loads==5);
    native_room_render_reset();assert(!native_room_render_stats.drawn);
    puts("PASS actual visible-room loading: fourth visible room renders past original budget; cached, failed, fog-hidden and invalid rooms preserve correct behavior");
}
