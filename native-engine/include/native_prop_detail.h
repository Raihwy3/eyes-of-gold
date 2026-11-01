#include "native_detail.h"
#include "native_settings.h"
#include "system.h"
#include "lv.h"
#include <math.h>
extern coord3d *bondviewGetCurrentPlayersPosition(void);
extern Mtxf *camGetWorldToScreenMtxf(void);
static float native_prop_detail_fade(PropRecord *prop,float radius) {
    /* Architectural occluders keep their authored portal/depth behaviour. */
    if(prop->parent || prop->type==PROP_TYPE_DOOR || prop->type==PROP_TYPE_VIEWER)return 1.f;
    if((prop->type==PROP_TYPE_OBJ || prop->type==PROP_TYPE_WEAPON) && prop->obj &&
       (prop->obj->type==PROPDEF_GLASS || prop->obj->type==PROPDEF_TINTED_GLASS))return 1.f;
    int choice=native_setting_view_distance();
    float end=native_detail_distance(choice);if(!end)return 1.f;
    coord3d *camera=bondviewGetCurrentPlayersPosition();if(!camera)return 1.f;
    float dx=prop->pos.x-camera->x,dy=prop->pos.y-camera->y,dz=prop->pos.z-camera->z;
    Mtxf *view=camGetWorldToScreenMtxf();if(!view)return 1.f;
    float fx=-view->m[0][2],fy=-view->m[1][2],fz=-view->m[2][2];
    float norm=sqrtf(fx*fx+fy*fy+fz*fz);if(!(norm>0))return 1.f;
    // Use the same forward-depth plane as the world fog, with scale removed.
    float depth=(dx*fx+dy*fy+dz*fz)/norm;
    return native_detail_fade(depth,radius,choice);
}

/* Called only at real render entries; fade queries do not inflate counts.
 * Each translation unit has its own counters, logged on the game thread. */
static int native_prop_detail_draw(PropRecord *prop,float radius,const char *kind) {
    static unsigned attempts,hidden,faded,last_tick;
    float fade=native_prop_detail_fade(prop,radius);
    attempts++;hidden+=fade<=0;faded+=fade>0 && fade<1;
    unsigned tick=(unsigned)g_GlobalTimer;
    if(tick-last_tick>=300){
        sysLogPrintf(LOG_NOTE,"Detail draws %s: checked=%u hidden=%u fading=%u lod=%d range=%d world-units",kind,attempts,hidden,faded,native_setting_lod(),(int)native_detail_distance(native_setting_view_distance()));
        attempts=hidden=faded=0;last_tick=tick;
    }
    return fade>0;
}
