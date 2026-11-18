#include <ultra64.h>
#include "bondtypes.h"
#include "model.h"
#include "native_settings.h"
#include "native_detail.h"
#include "native_object_batch.h"
#include "native_baked_lod.h"
#include "native_glass.h"
#include "native_scenery_scope.h"
#include "native_test_assert.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static Model actual_model;
static coord3d camera;
static int lod,glass_setting,draws;
static unsigned expected_tier;
unsigned native_object_lod_tier;
int native_setting_lod(void){return lod;}
int native_setting_lod_strength(void){return lod;}
int native_setting_glass(void){return glass_setting;}
coord3d *bondviewGetCurrentPlayersPosition(void){return &camera;}
f32 getinstsize(Model *instance){assert(instance==&actual_model);return 100.f;}
void subdraw(ModelRenderData *data,Model *instance) {
    (void)data;assert(instance==&actual_model);assert(native_object_lod_tier==expected_tier);draws++;
}
static void render(PropRecord *prop,ModelRenderData *mrData,int arg2,int destroyed) {
    ObjectRecord *obj=prop->obj;Model *model=&actual_model;
#include "native-prop-lod-body.h"
    assert(native_object_lod_tier==9);
}
int main(void) {
    const unsigned types[]={PROPDEF_PROP,PROPDEF_VEHICHLE,PROPDEF_DOOR,PROPDEF_GLASS,PROPDEF_TINTED_GLASS,PROPDEF_COLLECTABLE};
    for(lod=0;lod<4;lod++)for(int distance=100;distance<=2100;distance+=500)
    for(unsigned t=0;t<sizeof types/sizeof types[0];t++)for(int secondary=0;secondary<2;secondary++)
    for(int destroyed=0;destroyed<2;destroyed++)for(glass_setting=0;glass_setting<2;glass_setting++) {
        ObjectRecord object={0};object.type=types[t];ObjectRecord saved=object;
        PropRecord prop={0};prop.obj=&object;prop.pos.z=distance;PropRecord old_prop=prop;
        Gfx commands[8];memset(commands,0,sizeof commands);ModelRenderData data={0};data.gdl=commands;
        int pane=types[t]==PROPDEF_GLASS || types[t]==PROPDEF_TINTED_GLASS;
        int opaque=native_object_batch_eligible(types[t],secondary,destroyed);
        int eligible=!destroyed && (opaque || pane || types[t]==PROPDEF_DOOR);
        unsigned tier=eligible?native_detail_tier(distance-100.f,lod):0;
        int simple=pane && native_glass_lod_simple(glass_setting,tier);
        expected_tier=eligible?tier:9;native_object_lod_tier=9;int before=draws;
        render(&prop,&data,secondary,destroyed);assert(draws==before+1);
        assert(!memcmp(&object,&saved,sizeof object) && !memcmp(&prop,&old_prop,sizeof prop));
        unsigned scopes=0,details=0;
        for(Gfx *p=commands;p<data.gdl;p++) {
            unsigned op=p->words.w0>>24;
            if(op==NATIVE_GLASS_DETAIL_COMMAND){assert(pane);assert((p->words.w0&7)==(unsigned)(simple|(tier<<1)));details++;}
            if(op==NATIVE_SCENERY_COMMAND && (p->words.w0&7)){assert((p->words.w0&7)==(unsigned)(simple?5:types[t]==PROPDEF_VEHICHLE?4:2));scopes++;}
        }
        assert(details==(unsigned)pane && scopes==(unsigned)((simple && !destroyed)||opaque));
    }
    puts("PASS actual glass/door LOD consumer: typed model radius, distance/detail commands, ordered simple-glass/opaque-door scopes, secondary/destruction fallbacks and unchanged prop state");
}
