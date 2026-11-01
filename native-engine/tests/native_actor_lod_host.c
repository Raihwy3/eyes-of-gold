#include <ultra64.h>
#include "bondtypes.h"
#include "model.h"
#include "native_baked_lod.h"
#include "native_scenery_scope.h"
#include "native_detail.h"
#include "native_test_assert.h"
#include <math.h>
#include <stdio.h>
static Model model;
static ModelHitEntry entries;
static coord3d camera;
static int lod,size_calls,draws;
static unsigned expected_tier;
unsigned native_object_lod_tier;
int native_setting_lod(void){return lod;}
int native_setting_lod_strength(void){return lod;}
coord3d *bondviewGetCurrentPlayersPosition(void){return &camera;}
f32 getinstsize(Model *instance) {
    /* A ModelHitEntry list is deliberately a different object. The crashing
     * overlay passes that list here; verify identity before dereferencing. */
    assert(instance==&model);size_calls++;return 250.f;
}
static void drawjointlist(ModelRenderData *data,ModelHitEntry *list) {
    (void)data;assert(list==&entries);assert(native_object_lod_tier==expected_tier);draws++;
}
static void render(ChrRecord *chr,PropRecord *prop,int withalpha,int chrfadealpha,int spB8) {
    Gfx commands[4];ModelRenderData mrData={0};mrData.gdl=commands;
#include "native-actor-lod-body.h"
    if(native_actor_batch)assert((commands[0].words.w0&7)==(native_actor_faded?6:3));
    assert(native_object_lod_tier==7);
    assert(mrData.gdl==commands+(((!withalpha && chrfadealpha==255 && spB8==1) || (withalpha && spB8==3 && chrfadealpha>0))?2:0));
}
int main(void) {
    ChrRecord chr={0};PropRecord prop={0};chr.model=&model;chr.field_20=&entries;
    const int modes[][3]={{0,255,1},{1,255,1},{0,254,1},{0,255,0},{1,128,3},{1,255,3},{1,0,3},{1,255,2}};
    for(lod=0;lod<4;lod++)for(int distance=500;distance<=2000;distance+=500)for(unsigned mode=0;mode<8;mode++) {
        prop.pos.z=distance;native_object_lod_tier=7;size_calls=0;
        int eligible=mode==0 || mode==4 || mode==5;
        expected_tier=!eligible?7:native_detail_tier(distance-250.f,lod);
        int before=draws;render(&chr,&prop,modes[mode][0],modes[mode][1],modes[mode][2]);
        assert(draws==before+1 && size_calls==eligible);
    }
    puts("PASS actual actor LOD consumer: guard Model supplies radius, render-entry list stays separate, distance tiers and opaque/fade/secondary scope restoration");
}
