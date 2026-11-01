#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <math.h>
#include "native_effects.h"
#include "native_detail.h"
#include "native_settings.h"
#include "bgfog.h"
#include "lv.h"
#include "system.h"
extern coord3d *bondviewGetCurrentPlayersPosition(void);
extern Mtxf *camGetWorldToScreenMtxf(void);
extern s32 getPlayerCount(void);
static unsigned queries,culled,reduced,spawns_skipped,spawn_serial;
static float basis[3],forward[3];
static int basis_valid;
int native_effects_tier(const coord3d *pos,float radius) {
    if(!pos || getPlayerCount()!=1 || lvlGetCurrentStageToLoad()==LEVELID_TITLE)return 0;
    coord3d *eye=bondviewGetCurrentPlayersPosition();Mtxf *view=camGetWorldToScreenMtxf();
    if(!eye || !view || !__builtin_isfinite(pos->x) || !__builtin_isfinite(pos->y) || !__builtin_isfinite(pos->z) || !__builtin_isfinite(radius))return 0;
    if(radius<0)radius=0;
    queries++;
    int distance=native_setting_view_distance();
    float dx=pos->x-eye->x,dy=pos->y-eye->y,dz=pos->z-eye->z;
    if(!distance) {
        if(!fogPositionIsVisibleThroughFog((coord3d *)pos,radius)){culled++;return -1;}
    } else {
        float fx=-view->m[0][2],fy=-view->m[1][2],fz=-view->m[2][2];
        if(!basis_valid || fx!=basis[0] || fy!=basis[1] || fz!=basis[2]) {
            float norm=sqrtf(fx*fx+fy*fy+fz*fz);
            if(!(norm>0) || !__builtin_isfinite(norm)){basis_valid=0;return 0;}
            basis[0]=fx;basis[1]=fy;basis[2]=fz;
            forward[0]=fx/norm;forward[1]=fy/norm;forward[2]=fz/norm;basis_valid=1;
        }
        float depth=dx*forward[0]+dy*forward[1]+dz*forward[2];
        if(depth-radius>=native_detail_distance(distance)){culled++;return -1;}
    }
    int strength=native_setting_lod_strength();
    if(!strength)return 0;
    int tier=native_detail_tier(sqrtf(dx*dx+dy*dy+dz*dz)-radius,strength);
    reduced+=tier>0;return tier;
}
int native_effects_keep(int tier,unsigned index) {
    if(tier<0)return 0;
    unsigned mask=tier<=0?0:tier==1?1:tier==2?3:7;
    return (index&mask)==0;
}
int native_effects_spawn(const coord3d *pos,float radius) {
    int keep=native_effects_keep(native_effects_tier(pos,radius),spawn_serial++);
    spawns_skipped+=!keep;return keep;
}
int native_effects_beam(const coord3d *pos,const coord3d *delta,float length) {
    /* Whole-segment bounds retain tracers crossing the fog toward the player. */
    coord3d center=*pos;
    float half=fabsf(length)*.5f;
    center.x+=delta->x*length*.5f;center.y+=delta->y*length*.5f;center.z+=delta->z*length*.5f;
    float radius=half*sqrtf(delta->x*delta->x+delta->y*delta->y+delta->z*delta->z)+64.f;
    return native_effects_tier(&center,radius);
}
void native_effects_report(void) {
    sysLogPrintf(LOG_NOTE,"Effect LOD: queries=%u fogCulled=%u reduced=%u cosmeticSpawnsSkipped=%u",queries,culled,reduced,spawns_skipped);
    queries=culled=reduced=spawns_skipped=0;
}
