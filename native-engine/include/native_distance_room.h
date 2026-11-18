#include "native_detail.h"
#include "native_settings.h"
#include <math.h>
/* A near/current room stays full detail. Selecting on the nearest AABB point
 * avoids simplifying the far end of a room whose entrance is beside Bond. */
static unsigned native_room_detail_tier(int room) {
    if(room<=0 || room>=g_MaxNumRooms || room==g_CurrentPlayer->curRoomIndex)return 0;
    coord3d *eye=bondviewGetCurrentPlayersPosition();if(!eye)return 0;
    float scale=get_room_data_float2();if(!(scale>0))return 0;
    float distance2=0;
    for(unsigned i=0;i<3;i++) {
        float lo=g_BgRoomInfo[room].minbounds.f[i]*scale,hi=g_BgRoomInfo[room].maxbounds.f[i]*scale;
        if(!(lo<=hi))return 0;
        float delta=eye->f[i]<lo?lo-eye->f[i]:eye->f[i]>hi?eye->f[i]-hi:0;
        distance2+=delta*delta;
    }
    return native_detail_tier(sqrtf(distance2),native_setting_lod_strength());
}
/* Nearest point of the entire AABB along camera forward. A room intersecting
 * the fog plane is retained; per-triangle clipping handles its far section. */
static int native_distance_room_visible(int room) {
    float end=native_detail_distance(native_setting_view_distance());
    if(!end || room<=0 || room>=g_MaxNumRooms || room==g_CurrentPlayer->curRoomIndex)return 1;
    const coord3d *lo=&g_BgRoomInfo[room].minbounds,*hi=&g_BgRoomInfo[room].maxbounds;
    coord3d *eye=bondviewGetCurrentPlayersPosition();Mtxf *view=camGetWorldToScreenMtxf();
    if(!eye || !view)return 1;
    float forward[3]={-view->m[0][2],-view->m[1][2],-view->m[2][2]};
    float norm=sqrtf(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
    if(!(norm>0))return 1;
    float room_scale=get_room_data_float2();if(!(room_scale>0))return 1;
    float nearest=0;
    for(int i=0;i<3;i++){
        if(!(lo->f[i]<=hi->f[i]))return 1;
        nearest+=((forward[i]>=0?lo->f[i]:hi->f[i])*room_scale-eye->f[i])*forward[i];
    }
    return nearest<=end*norm;
}
