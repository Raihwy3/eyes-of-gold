#include <ultra64.h>
#include <bondaicommands.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "chr.h"
#include "chraction.h"
#include "chrai.h"
#include "chraidata.h"
#include "bondview.h"
#include "bgfog.h"
#include "lv.h"
#include "objecthandler.h"
#include "propobj.h"
#include "stan.h"
#include "drawdistgameplay.h"
#include "native_test_assert.h"

s32 g_GlobalTimer,g_ClockTimer=1;
static ChrRecord guards[8];
ChrRecord *g_ChrSlots=guards;
static PropRecord props[8],bond;
static Model models[8];
static StandTile tiles[2];
static Mtxf view;
static AIRecord lists[4];
AIListRecord g_GlobalAILists[]={
    {&lists[0],GAILIST_STANDARD_GUARD},
    {&lists[1],GAILIST_RUN_TO_BOND},
    {&lists[2],GAILIST_PLAY_IDLE_ANIMATION},
    {NULL,0}
};
static int wall_clear,ray_calls,full_ticks,ai_calls,action_calls,freed;
static int visibility_gate,ai_hz=30,authored_visible=1;
s32 fogPositionIsVisibleThroughFog(coord3d *pos,f32 radius){(void)pos;assert(radius==20.f);return authored_visible;}
int native_setting_guard_visibility(void){return visibility_gate;}
int native_setting_guard_ai_hz(void){return ai_hz;}
static int stage=LEVELID_DAM,players=1,range=3,on_screen;
static coord3d eye;
s32 get_numguards(void){return 8;}
s32 getPlayerCount(void){return players;}
s32 lvlGetCurrentStageToLoad(void){return stage;}
int native_setting_view_distance(void){return range;}
PropRecord *getCurrentPlayerProp(void){return &bond;}
Mtxf *camGetWorldToScreenMtxf(void){return &view;}
coord3d *bondviewGetCurrentPlayersPosition(void){return &eye;}
bool posIsOnScreen(PropRecord *p,coord3d *pos,f32 radius,bool unused){(void)p;(void)pos;(void)radius;(void)unused;return on_screen;}
f32 fogGetScaledFarFogIntensitySquared(void){return 4000.f*4000.f;}
f32 getinstsize(Model *model){(void)model;return 20.f;}
void chrSetMoving(ChrRecord *chr,bool unset){if(unset)chr->hidden&=~CHRHIDDEN_MOVING;else chr->hidden|=CHRHIDDEN_MOVING;}
void bondviewUpdateGuardTankFlagsRelated(PropRecord *p,s32 flag){(void)p;(void)flag;}
s32 stanTestLineUnobstructed(StandTile **tile,f32 x,f32 z,f32 bx,f32 bz,int types,f32 h,f32 a,f32 b,f32 c){
    (void)x;(void)z;(void)bx;(void)bz;(void)h;(void)a;(void)b;(void)c;
    assert(!(types&CDTYPE_CHRS));ray_calls++;if(wall_clear)*tile=bond.stan;return wall_clear;
}
void portPropSetGameplayOnScreen(PropRecord *p,int visible){(void)p;assert(!visible);}
void sub_GAME_7F06B248(ModelHitEntry *entry){assert(entry);freed++;}
void chrlvMergeKneelToStand(ChrRecord *chr,f32 t){(void)chr;(void)t;}
void ai(void *entity,s32 type){assert(entity && type==PROP_TYPE_CHR);ai_calls++;}
static void native_test_action(ChrRecord *chr){(void)chr;action_calls++;}
static s32 native_chr_tick_impl(PropRecord *prop){full_ticks++;chrlvActionTick(prop->chr);return TICKOP_NONE;}
#include "native_guard_activity_impl.h"
#include "native_guard_profile.h"
#include "activity-functions.h"
uint64_t sysGetMicroseconds(void){return 1;}
void sysLogPrintf(enum LogLevel level,const char *fmt,...){(void)level;(void)fmt;}

static void reset(void){
    memset(guards,0,sizeof guards);memset(props,0,sizeof props);
    memset(models,0,sizeof models);memset(&bond,0,sizeof bond);memset(&view,0,sizeof view);
    view.m[0][0]=view.m[1][1]=view.m[2][2]=view.m[3][3]=1;
    bond.stan=&tiles[1];g_GlobalTimer=100;g_ClockTimer=1;
    authored_visible=1;visibility_gate=0;ai_hz=30;stage=LEVELID_DAM;players=1;range=3;wall_clear=1;on_screen=0;
    memset(&eye,0,sizeof eye);
    ray_calls=full_ticks=ai_calls=action_calls=freed=0;
    for(int i=0;i<8;i++){
        guards[i].chrnum=i;guards[i].model=&models[i];guards[i].prop=&props[i];
        guards[i].ailist=&lists[0];guards[i].aireturnlist=-1;
        guards[i].actiontype=ACT_STAND;guards[i].chrheight=160;
        props[i].type=PROP_TYPE_CHR;props[i].chr=&guards[i];props[i].stan=&tiles[0];
        props[i].pos.z=-600;
    }
    native_guard_activity_reset();
}
int main(void){
    reset();props[0].pos.z=-1800;
    ModelHitEntry old={0};guards[0].field_20=&old;guards[0].sleep=10;
    guards[0].hidden=CHRHIDDEN_TIMER_ACTIVE;props[0].flags|=PROPFLAG_ONSCREEN;
    for(int i=0;i<120;i++,g_GlobalTimer++){
        assert(chrTick(&props[0])==TICKOP_NONE);
        assert(!native_guard_activity_awake(&guards[0]));
    }
    assert(!full_ticks && !ai_calls && !action_calls && !ray_calls && freed==1);
    assert(props[0].pos.z==-1800 && guards[0].timer60==0 && guards[0].sleep==10);
    assert(!guards[0].field_20 && !(props[0].flags&PROPFLAG_ONSCREEN));
    g_GlobalTimer++;props[0].pos.z=-1219;assert(native_guard_activity_awake(&guards[0]));
    chrTick(&props[0]);assert(full_ticks==1 && ai_calls==0 && guards[0].sleep==9);

    reset();wall_clear=0;
    for(int i=0;i<60;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(ray_calls==4 && !full_ticks && !ai_calls && !action_calls);
    wall_clear=1;
    for(int i=0;i<15;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(full_ticks>0); /* Door opening wakes within the probe interval. */

    /* A blocked head-to-Bond ray does not mean the guard model is hidden.
     * Exercise the actual tick wrapper: it must keep model/held-item data. */
    reset();wall_clear=0;on_screen=1;guards[0].field_20=&old;
    PropRecord held={0};held.flags=PROPFLAG_ONSCREEN;guards[0].weapons_held[GUNRIGHT]=&held;
    props[0].flags=PROPFLAG_ONSCREEN;
    for(int i=0;i<60;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(full_ticks==60 && ai_calls==30 && !freed && !ray_calls);
    assert(guards[0].field_20==&old && (props[0].flags&PROPFLAG_ONSCREEN) && (held.flags&PROPFLAG_ONSCREEN));
    reset();wall_clear=0;assert(!native_guard_activity_awake(&guards[0]));
    g_GlobalTimer++;on_screen=1;chrTick(&props[0]);assert(full_ticks==1 && ray_calls==1);
    reset();wall_clear=0;on_screen=1;props[0].pos.z=-1800;
    assert(!native_guard_activity_awake(&guards[0]));
    g_GlobalTimer++;props[0].pos.z=-1219;chrTick(&props[0]);assert(full_ticks==1);
    /* Camera offset, including a camera away from Bond, matches render fog. */
    reset();on_screen=1;props[0].pos.z=-1800;eye.z=-1000;
    assert(native_guard_activity_awake(&guards[0]));
    reset();on_screen=1;range=0;props[0].pos.z=-5000;
    assert(native_guard_activity_awake(&guards[0]));

    reset();
    for(int i=0;i<60;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(full_ticks==60 && action_calls==60 && ai_calls==30);
    reset();g_ClockTimer=2;
    for(int i=0;i<30;i++,g_GlobalTimer+=2)chrTick(&props[0]);
    assert(full_ticks==30 && action_calls==30 && ai_calls==30);

    reset();wall_clear=0;guards[0].ailist=&lists[1];
    for(int i=0;i<60;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(full_ticks==60 && ai_calls==30 && !ray_calls); /* Pursuit stays live. */
    props[0].pos.z=-1800;chrTick(&props[0]);assert(full_ticks==60); /* Fog still sleeps pursuit. */
    reset();wall_clear=0;props[0].pos.z=-1800;guards[0].ailist=&lists[3];
    for(int i=0;i<60;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(!full_ticks && !ai_calls); /* Fog freezes physical scripted guards too. */
    props[0].pos.z=-600;g_GlobalTimer++;chrTick(&props[0]);assert(full_ticks==1 && ai_calls==1);
    reset();props[0].pos.z=-1800;guards[0].aireturnlist=0x401;
    assert(!native_guard_activity_awake(&guards[0]) && !native_guard_activity_ai_due(&guards[0]));
    props[0].pos.z=-600;assert(native_guard_activity_awake(&guards[0]));
    assert(native_guard_activity_ai_due(&guards[0]));assert(native_guard_activity_ai_due(&guards[0]));

    reset();wall_clear=0;assert(!native_guard_activity_awake(&guards[0]));
    guards[0].lastheartarget60=g_GlobalTimer;assert(native_guard_activity_awake(&guards[0]));
    reset();wall_clear=0;assert(!native_guard_activity_awake(&guards[0]));
    guards[0].damage=1;assert(native_guard_activity_awake(&guards[0]));
    reset();wall_clear=0;assert(!native_guard_activity_awake(&guards[0]));
    guards[0].actiontype=ACT_DIE;assert(native_guard_activity_awake(&guards[0]));
    reset();wall_clear=0;props[0].pos.z=-100;assert(native_guard_activity_awake(&guards[0]));assert(!ray_calls);

    reset();wall_clear=0;
    for(int i=0;i<8;i++)native_guard_activity_slots[i].probe_tick=0;
    /* First 15 ticks distribute initial probes; no frame exceeds the budget. */
    for(int t=0;t<30;t++,g_GlobalTimer++){
        int before=ray_calls;for(int i=0;i<8;i++)native_guard_activity_awake(&guards[i]);assert(ray_calls-before<=2);
    }
    reset();props[0].pos.z=-1800;assert(!native_guard_activity_awake(&guards[0]));
    native_guard_activity_reset();props[0].pos.z=-600;assert(native_guard_activity_awake(&guards[0]));
    stage=LEVELID_TITLE;props[0].pos.z=-1800;assert(native_guard_activity_awake(&guards[0]));
    for(int rate=0;rate<3;rate++)for(int step=1;step<=2;step++) {
        static const int rates[]={30,15,5};reset();ai_hz=rates[rate];g_ClockTimer=step;on_screen=1;
        for(int tick=0;tick<60;tick+=step,g_GlobalTimer+=step)chrTick(&props[0]);
        assert(ai_calls==rates[rate] && full_ticks==60/step && action_calls==60/step);
    }
    reset();ai_hz=5;on_screen=1;chrTick(&props[0]);assert(ai_calls==1);
    g_GlobalTimer++;chrTick(&props[0]);assert(ai_calls==1);
    guards[0].damage=1;chrTick(&props[0]);assert(ai_calls==2);
    ai_hz=30;g_GlobalTimer+=2;chrTick(&props[0]);assert(ai_calls==3);
    g_GlobalTimer+=600;chrTick(&props[0]);assert(ai_calls==4); /* No catch-up burst. */
    reset();ai_hz=5;guards[0].ailist=&lists[3];
    for(int i=0;i<60;i++,g_GlobalTimer++)chrTick(&props[0]);assert(ai_calls==60);

    reset();visibility_gate=1;wall_clear=0;
    for(int i=0;i<60;i++,g_GlobalTimer++) {
        chrTick(&props[0]);assert(!native_guard_activity_sight_allowed(&guards[0]));
    }
    assert(!ray_calls && !full_ticks); /* Offscreen: no visibility/perception probes. */
    on_screen=1;guards[0].field_20=&old;props[0].flags=PROPFLAG_ONSCREEN;
    held.flags=PROPFLAG_ONSCREEN;guards[0].weapons_held[GUNRIGHT]=&held;
    for(int i=0;i<60;i++,g_GlobalTimer++) {
        chrTick(&props[0]);assert(!native_guard_activity_sight_allowed(&guards[0]));
    }
    assert(ray_calls==4 && full_ticks==60 && !freed);
    assert(guards[0].field_20==&old && (props[0].flags&PROPFLAG_ONSCREEN) && (held.flags&PROPFLAG_ONSCREEN));
    wall_clear=1;int saw=0;
    for(int i=0;i<15;i++,g_GlobalTimer++){chrTick(&props[0]);saw|=native_guard_activity_sight_allowed(&guards[0]);}
    assert(saw);on_screen=0;assert(!native_guard_activity_sight_allowed(&guards[0]));
    visibility_gate=0;assert(native_guard_activity_sight_allowed(&guards[0]));
    visibility_gate=1;guards[0].ailist=&lists[1];assert(native_guard_activity_sight_allowed(&guards[0]));
    guards[0].ailist=&lists[3];assert(native_guard_activity_sight_allowed(&guards[0]));
    guards[0].ailist=&lists[0];guards[0].lastheartarget60=g_GlobalTimer;assert(native_guard_activity_sight_allowed(&guards[0]));
    reset();visibility_gate=1;on_screen=1;
    for(int tick=0;tick<60;tick++,g_GlobalTimer++) {
        int before=ray_calls;for(int i=0;i<8;i++)native_guard_activity_sight_allowed(&guards[i]);
        assert(ray_calls-before<=2);
    }
    /* All physical guard states freeze beyond fog, even when an earlier
     * on-screen verdict, alert or mission script would normally wake them. */
    for(int mode=0;mode<8;mode++) {
        reset();on_screen=1;props[0].pos.z=-1800;guards[0].timer60=17;guards[0].sleep=33;
        guards[0].hidden=CHRHIDDEN_TIMER_ACTIVE;
        if(mode==1)guards[0].ailist=&lists[1];
        if(mode==2)guards[0].ailist=&lists[3];
        if(mode==3)guards[0].aireturnlist=0x401;
        if(mode==4){guards[0].lastheartarget60=g_GlobalTimer;guards[0].lastseetarget60=g_GlobalTimer;}
        if(mode==5){guards[0].damage=1;guards[0].chrflags|=CHRFLAG_NEAR_MISS;guards[0].hidden|=CHRHIDDEN_ALERT_GUARD_RELATED;}
        if(mode==6)guards[0].actiontype=ACT_DIE;
        if(mode==7){guards[0].actiontype=ACT_ANIM;guards[0].hidden|=CHRHIDDEN_FIRE_WEAPON_RIGHT;}
        PropRecord attached={0};attached.type=PROP_TYPE_WEAPON;attached.parent=&props[0];
        PropRecord nested={0};nested.type=PROP_TYPE_OBJ;nested.parent=&attached;
        PropRecord pickup={0};pickup.type=PROP_TYPE_WEAPON;
        for(int i=0;i<60;i++,g_GlobalTimer++) {
            chrTick(&props[0]);assert(!native_guard_activity_ai_due(&guards[0]));
            assert(!native_guard_activity_sight_allowed(&guards[0]));
            assert(!native_guard_activity_drawable(&props[0]));
            assert(!native_guard_activity_drawable(&attached) && !native_guard_activity_drawable(&nested));
            assert(native_guard_activity_drawable(&pickup));
        }
        assert(!full_ticks && !ai_calls && !action_calls && !ray_calls);
        assert(guards[0].timer60==17 && guards[0].sleep==33 && props[0].pos.z==-1800);
        eye.z=-1000; /* Wake on camera movement in the same simulation tick. */
        assert(native_guard_activity_awake(&guards[0]) && native_guard_activity_drawable(&nested));
        chrTick(&props[0]);assert(full_ticks==1 && ai_calls==0 && action_calls==0 && guards[0].sleep==32);
    }
    reset();props[0].pos.z=-1220;assert(native_guard_activity_beyond_fog(&guards[0]));
    props[0].pos.z=-1219;assert(!native_guard_activity_beyond_fog(&guards[0]));
    props[0].pos.z=-1800;assert(native_guard_activity_beyond_fog(&guards[0]));
    range=1;assert(!native_guard_activity_beyond_fog(&guards[0])); /* Same tick setting change. */
    range=3;view.m[2][2]=3;assert(native_guard_activity_beyond_fog(&guards[0])); /* View scale normalized. */
    eye.z=-1000;assert(!native_guard_activity_beyond_fog(&guards[0]));
    reset();range=0;authored_visible=0;on_screen=1;
    assert(native_guard_activity_beyond_fog(&guards[0]) && !native_guard_activity_awake(&guards[0]));
    authored_visible=1;assert(native_guard_activity_awake(&guards[0]));
    reset();props[0].pos.z=-1800;view.m[2][2]=0;assert(!native_guard_activity_beyond_fog(&guards[0]));
    reset();wall_clear=0;guards[0].hidden=CHRHIDDEN_TIMER_ACTIVE;guards[0].sleep=10;
    for(int i=0;i<10;i++,g_GlobalTimer++)chrTick(&props[0]);
    assert(guards[0].timer60==10 && !guards[0].sleep); /* Inside-fog wall sleep keeps existing timer policy. */
    puts("PASS hard fog freeze: no guard/action/AI/ray/timer work in every physical guard state, held/nested visuals culled, exact bounding sphere and authored fog, live camera/range changes and one-step wake");
    puts("PASS experimental guard LOS: no offscreen probes, bounded wall/door rechecks, visible models retained, pursuit/script/hearing exemptions; 30/15/5 Hz at 30/60 render ticks, live changes and damage wake");
    puts("PASS actual guard activity: visible guards survive blocked sight rays, camera turns/offsets and Original fog; wake before fog boundary; offscreen sleep, pursuit/scripts, 30Hz AI, 4Hz probes, damage/hearing/door wake, timers and cleanup");
}
