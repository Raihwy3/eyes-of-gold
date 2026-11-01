#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "bgfog.h"
#include "lv.h"
#include "system.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "native_test_assert.h"
static coord3d eye;static Mtxf view;
static int players=1,stage=LEVELID_DAM,distance=3,strength=0,authored=1,no_camera;
s32 getPlayerCount(void){return players;}
s32 lvlGetCurrentStageToLoad(void){return stage;}
coord3d *bondviewGetCurrentPlayersPosition(void){return no_camera?NULL:&eye;}
Mtxf *camGetWorldToScreenMtxf(void){return &view;}
s32 fogPositionIsVisibleThroughFog(coord3d *p,f32 radius){(void)p;(void)radius;return authored;}
int native_setting_view_distance(void){return distance;}
int native_setting_lod_strength(void){return strength;}
void sysLogPrintf(enum LogLevel l,const char *fmt,...){(void)l;(void)fmt;}
#include "../src/native_effects.c"
int main(void){
 view.m[2][2]=1;coord3d p={.z=-1220};
 assert(native_effects_tier(&p,20)==-1);
 p.z=-1219;assert(native_effects_tier(&p,20)==0);
 p.z=-1800;view.m[2][2]=3;assert(native_effects_tier(&p,20)==-1);
 eye.z=-1000;assert(native_effects_tier(&p,20)==0);eye.z=0;
 distance=1;assert(native_effects_tier(&p,0)==0);
 strength=1;p.z=-1000;assert(native_effects_tier(&p,0)==1);
 p.z=-1800;assert(native_effects_tier(&p,0)==2);
 p.z=-3500;assert(native_effects_tier(&p,0)==3);
 for(int tier=0;tier<=3;tier++){unsigned keep=0;for(unsigned i=0;i<64;i++)keep+=native_effects_keep(tier,i);assert(keep==(64u>>tier));}
 assert(!native_effects_keep(-1,0));
 distance=0;authored=0;assert(native_effects_tier(&p,0)==-1);
 authored=1;strength=0;assert(native_effects_tier(&p,0)==0);
 distance=3;coord3d delta={.z=1};p.z=-1800;
 assert(native_effects_beam(&p,&delta,1200)>=0); /* Crossing shot remains visible. */
 p.z=-3000;assert(native_effects_beam(&p,&delta,100)==-1);
 for(int i=0;i<16;i++)assert(!native_effects_spawn(&p,64));
 distance=1;strength=1;p.z=-1800;spawn_serial=0;int kept=0;
 for(int i=0;i<64;i++)kept+=native_effects_spawn(&p,0);assert(kept==16);
 players=2;assert(native_effects_tier(&p,0)==0);players=1;
 stage=LEVELID_TITLE;assert(native_effects_tier(&p,0)==0);stage=LEVELID_DAM;
 no_camera=1;assert(native_effects_tier(&p,0)==0);no_camera=0;
 p.x=__builtin_nanf("");assert(native_effects_tier(&p,0)==0);p.x=0;
 view.m[2][2]=0;assert(native_effects_tier(&p,0)==0);
 puts("PASS effect LOD: three density tiers, exact/custom/authored fog, model radius, scaled/live cameras, crossing tracers, fog spawn suppression, Original settings and title/multiplayer/invalid-input fallbacks");
}
