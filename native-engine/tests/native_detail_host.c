#include <ultra64.h>
#include "bondtypes.h"
#include "objecthandler.h"
#include "model.h"
#include "bgfog.h"
#include "bg.h"
#include "player.h"
#include "native_distance_fog.h"
#include "native_settings.h"
#include "native_detail.h"
#include "native_object_batch.h"
#include "native_glass.h"
#include "system.h"
#include "native_test_assert.h"
#include <string.h>
#include <stdarg.h>
static union ModelRwData rw;
static Mtxf test_matrix;
s32 g_ModelDistanceDisabled;
f32 g_ModelDistanceScale=1.f;
union ModelRwData *modelGetNodeRwData(Model *model,ModelNode *node){(void)model;(void)node;return &rw;}
Mtxf *modelFindNodeMtx(Model *model,ModelNode *node,s32 index){(void)model;(void)node;(void)index;return &test_matrix;}
f32 getPlayer_c_lodscalez(void){return 1.f;}
#include "native-detail-engine.h"
s32 g_GlobalTimer;
static char detail_log[512];
void sysLogPrintf(enum LogLevel level,const char *format,...);
static coord3d camera;
static Mtxf camera_basis;
Mtxf *camGetWorldToScreenMtxf(void){return &camera_basis;}
s_room_info g_BgRoomInfo[3];s32 g_MaxNumRooms=3;
static float room_scale=1;
f32 get_room_data_float2(void){return room_scale;}
static struct player player;
struct player *g_CurrentPlayer=&player;
coord3d *bondviewGetCurrentPlayersPosition(void){return &camera;}
#include "native_prop_detail.h"
#include "native_distance_room.h"
static NearFogRecord *fog_data;
NearFogRecord *fogGetNearFogValuesP(void){return fog_data;}
#include "native-detail-fade.h"
void sysLogPrintf(enum LogLevel level,const char *format,...){
    (void)level;va_list args;va_start(args,format);vsnprintf(detail_log,sizeof detail_log,format,args);va_end(args);
}
int main(void){
    for(unsigned type=0;type<PROPDEF_MAX;type++) {
        assert(native_object_batch_eligible(type,0,0)==(type==PROPDEF_PROP || type==PROPDEF_VEHICHLE || type==PROPDEF_DOOR));
        assert(!native_object_batch_eligible(type,1,0));assert(!native_object_batch_eligible(type,0,1));
    }
    for(unsigned tier=0;tier<4;tier++) {
        assert(native_glass_lod_simple(1,tier));assert(native_glass_lod_simple(0,tier)==(tier>=2));
    }
    puts("PASS object batching eligibility: intact opaque props/vehicles/doors, secondary/destroyed/other model types excluded; glass LOD keeps near reflections and flat far panes");
    native_settings_init(NULL);camera_basis.m[2][2]=-1;
    ModelNode high_child={0},low_child={0},high={0},low={0};
    union ModelRoData high_data={0},low_data={0};
    high.Data=&high_data;low.Data=&low_data;
    high_data.LOD=(ModelRoData_LODRecord){0,1000,&high_child,0,0};
    low_data.LOD=(ModelRoData_LODRecord){1000,100000,&low_child,0,0};
    Model model={0};model.scale=1;
    for(int setting=0;setting<4;setting++){
        native_setting_set_lod(setting);
        float scale=native_detail_lod_scale(setting);
        for(int z=10;z<9000;z+=10){
            test_matrix.m[3][2]=-z;
            modelUpdateDistanceRelations(&model,&high);int detailed=rw.LOD.visible;
            assert(detailed==(z*scale<=1000));
            modelUpdateDistanceRelations(&model,&low);
            assert(detailed!=rw.LOD.visible && low.Child==(detailed?NULL:&low_child));
        }
        g_ModelDistanceDisabled=1;modelUpdateDistanceRelations(&model,&high);assert(rw.LOD.visible);
        g_ModelDistanceDisabled=0;
    }
    PropRecord prop={0};ObjectRecord object={0};prop.type=PROP_TYPE_OBJ;prop.obj=&object;
    for(int choice=0;choice<4;choice++){
        native_setting_set_view_distance(choice);
        float previous=1;
        for(int depth=0;depth<10000;depth+=5){
            prop.pos.z=depth;prop.zDepth=depth*.01f;PropRecord saved=prop;
            float fade=native_prop_detail_fade(&prop,100);
            assert(fade>=0 && fade<=previous);previous=fade;
            assert(!memcmp(&saved,&prop,sizeof prop));
            assert(chrobjFogVisRangeRelated(&prop,100)==fade); // no authored fade
        }
        if(choice)assert(previous==0);else assert(previous==1);
        float end=native_detail_distance(choice);
        if(choice){assert(native_detail_fade(end,100,choice)>0);assert(native_detail_fade(end+100,100,choice)==0);}
    }
    // Changing scaled depth cannot change the actual world distance decision.
    native_setting_set_view_distance(3);prop.pos.z=1500;prop.zDepth=1;
    object.type=0;prop.type=PROP_TYPE_OBJ;assert(native_prop_detail_fade(&prop,100)==0);
    prop.zDepth=999999;assert(native_prop_detail_fade(&prop,100)==0);
    prop.pos.z=100;assert(native_prop_detail_fade(&prop,100)==1);
    prop.pos.z=99999;prop.zDepth=99999;
    g_GlobalTimer=300;assert(!native_prop_detail_draw(&prop,100,"test"));
    assert(strstr(detail_log,"checked=1 hidden=1 fading=0") && strstr(detail_log,"range=1200 world-units"));
    PropRecord parent={0};prop.parent=&parent;assert(native_prop_detail_fade(&prop,0)==1);prop.parent=NULL;
    prop.type=PROP_TYPE_DOOR;assert(native_prop_detail_fade(&prop,0)==1);
    prop.type=PROP_TYPE_VIEWER;assert(native_prop_detail_fade(&prop,0)==1);
    prop.type=PROP_TYPE_OBJ;object.type=PROPDEF_GLASS;assert(native_prop_detail_fade(&prop,0)==1);
    object.type=PROPDEF_TINTED_GLASS;assert(native_prop_detail_fade(&prop,0)==1);
    // Long rooms spanning the fog plane stay visible even with a distant centre.
    native_setting_set_view_distance(3);camera_basis.m[2][2]=-4;
    g_BgRoomInfo[1].minbounds=(coord3d){{-100,-100,1000}};
    g_BgRoomInfo[1].maxbounds=(coord3d){{100,100,9000}};
    assert(native_distance_room_visible(1));
    g_BgRoomInfo[1].minbounds.z=1300;assert(!native_distance_room_visible(1));
    room_scale=.5f;assert(native_distance_room_visible(1));room_scale=1;
    player.curRoomIndex=1;assert(native_distance_room_visible(1));player.curRoomIndex=0;
    native_setting_set_view_distance(0);assert(native_distance_room_visible(1));
    for(int scale=1;scale<=4;scale++){
        struct NativeDistanceFog fog;
        double n=5.0/scale,f=15000.0/scale;
        assert(native_distance_fog_plan(n,f,1200,0x103060ff,&fog));
        for(int z=1;z<=1500;z++){
            float clipz=(float)(((f+n)/(f-n))*z-2*f*n/(f-n));
            float factor=native_distance_fog_factor(&fog,clipz,z);
            assert(factor>=0 && factor<=255);
            if(z<770)assert(factor<.1f);
            if(z>=1201)assert(factor>254.9f);
        }
    }
    puts("PASS distance fog math: all camera scales, matching start/end, conservative whole-room AABB rejection, current/crossing rooms retained");
    puts("PASS native detail: original LOD branch selection at 1x/2x/4x/8x, disabled menu path, world-space forward-depth fades independent of camera scaling, unchanged prop state, doors/glass/viewer exemptions");
}
