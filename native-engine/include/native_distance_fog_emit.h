#include "native_lod_scope.h"
/* Included after original engine types/prototypes. No gameplay state changes. */
#include "native_distance_fog.h"
#include "native_detail.h"
#include "native_settings.h"
#include "dyn.h"
#include "fr.h"
#include "bgfog.h"
#include "bg.h"
static int native_distance_fog_current(struct NativeDistanceFog *out) {
    float end=native_detail_distance(native_setting_view_distance());if(!end)return 0;
    float planes[2];viGetZRange(planes);
    float scale=bgGetLevelVisibilityScale();if(!(scale>0))return 0;
    CurrentEnvironmentRecord *env=fogGetCurrentEnvironmentp();if(!env)return 0;
    unsigned colour=((unsigned)env->Red<<24)|((unsigned)env->Green<<16)|((unsigned)env->Blue<<8)|255;
    return native_distance_fog_plan(planes[0]/scale,planes[1]/scale,end,colour,out);
}
static Gfx *native_distance_fog_begin(Gfx *gdl) {
    struct NativeLodScope *lod=dynAllocate(sizeof *lod);
    Mtxf *basis=camGetWorldToScreenMtxf();
    lod->world_to_view=basis?sqrtf(basis->m[0][2]*basis->m[0][2]+basis->m[1][2]*basis->m[1][2]+basis->m[2][2]*basis->m[2][2]):0;
    native_lod_scope(gdl++,lod);
    struct NativeDistanceFog value,*params=NULL;
    if(native_distance_fog_current(&value)){params=dynAllocate(sizeof *params);*params=value;}
    native_distance_fog_command(gdl++,params);return gdl;
}
static Gfx *native_distance_fog_end(Gfx *gdl) {
    native_distance_fog_command(gdl++,(struct NativeDistanceFog *)NULL);
    native_lod_scope(gdl++,(struct NativeLodScope *)NULL);return gdl;
}
static Gfx *native_distance_fog_backdrop(Gfx *gdl) {
    CurrentEnvironmentRecord *env=fogGetCurrentEnvironmentp();
    gDPPipeSync(gdl++);gDPSetCycleType(gdl++,G_CYC_FILL);
    gDPSetRenderMode(gdl++,G_RM_NOOP,G_RM_NOOP2);
    gdl=viSetFillColor(gdl,env->Red,env->Green,env->Blue);
    gDPFillRectangle(gdl++,viGetViewLeft(),viGetViewTop(),viGetViewLeft()+viGetViewWidth()-1,viGetViewTop()+viGetViewHeight()-1);
    gDPPipeSync(gdl++);gDPSetCycleType(gdl++,G_CYC_1CYCLE);
    return gdl;
}
