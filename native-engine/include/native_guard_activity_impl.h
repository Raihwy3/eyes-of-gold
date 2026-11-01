/* Included once in chraction.c, after the original engine declarations.
 * No original content records: only native scheduler state and policy code. */
#include "native_guard_activity.h"
#include "native_detail.h"
#include "native_settings.h"
#include "chraidata.h"
#include "system.h"
#include <string.h>
#include <stdint.h>

struct NativeGuardActivity {
    ChrRecord *owner;
    Model *model;
    PropRecord *prop;
    int chrnum, decision_tick, sleep_tick, ai_tick, probe_tick;
    int awake, clear, fog_sleep;
    float damage;
};
/* More entries than the supported stage requires. Collisions fail open. */
static struct NativeGuardActivity native_guard_activity_slots[256];
static int native_guard_activity_frame, native_guard_activity_probes;
static unsigned native_guard_activity_awake_count, native_guard_activity_sleep_count;
static unsigned native_guard_activity_ai_run, native_guard_activity_ai_skip;
static unsigned native_guard_activity_ray_count, native_guard_activity_fog_count;
static int native_guard_activity_report_tick;
static float native_guard_activity_fog_end, native_guard_activity_forward[3];
static float native_guard_activity_basis[3];
static int native_guard_activity_context_choice,native_guard_activity_context_valid;

void native_guard_activity_reset(void) {
    memset(native_guard_activity_slots,0,sizeof native_guard_activity_slots);
    native_guard_activity_frame=-1;
    native_guard_activity_report_tick=0;
    native_guard_activity_context_valid=0;
    native_guard_activity_awake_count=native_guard_activity_sleep_count=0;
    native_guard_activity_ai_run=native_guard_activity_ai_skip=0;
    native_guard_activity_ray_count=native_guard_activity_fog_count=0;
}

static struct NativeGuardActivity *native_guard_activity_slot(ChrRecord *chr) {
    uintptr_t at=(uintptr_t)chr,base=(uintptr_t)g_ChrSlots;
    if(!chr || !g_ChrSlots || at<base || (at-base)%sizeof *chr)return NULL;
    uintptr_t index=(at-base)/sizeof *chr;
    if(index>=256 || index>=(unsigned)get_numguards())return NULL;
    struct NativeGuardActivity *s=&native_guard_activity_slots[index];
    if(s->owner!=chr || s->model!=chr->model || s->prop!=chr->prop || s->chrnum!=chr->chrnum) {
        memset(s,0,sizeof *s);
        s->owner=chr;s->model=chr->model;s->prop=chr->prop;s->chrnum=chr->chrnum;
        s->decision_tick=s->sleep_tick=s->ai_tick=-1;
        s->probe_tick=g_GlobalTimer+(int)(index%15);
        s->awake=s->clear=1;s->damage=chr->damage;
    }
    return s;
}

/* Same boundary as rendering: custom forward-depth fog, or the original fog
 * visibility test. A model intersecting the visible side stays live. No rays,
 * AI-list lookup, model tick or timer catch-up is performed here. */
int native_guard_activity_beyond_fog(ChrRecord *chr) {
    if(!chr || !chr->model || !chr->prop || chr->prop->type!=PROP_TYPE_CHR ||
       lvlGetCurrentStageToLoad()==LEVELID_TITLE || getPlayerCount()!=1)return 0;
    uintptr_t at=(uintptr_t)chr,base=(uintptr_t)g_ChrSlots;
    if(!base || at<base || (at-base)%sizeof *chr ||
       (at-base)/sizeof *chr>=(unsigned)get_numguards())return 0;
    coord3d *eye=bondviewGetCurrentPlayersPosition();
    Mtxf *view=camGetWorldToScreenMtxf();
    if(!eye || !view)return 0;
    float radius=getinstsize(chr->model);if(radius<0)radius=0;
    int choice=native_setting_view_distance();
    if(!choice)return !fogPositionIsVisibleThroughFog(&chr->prop->pos,radius);
    float fx=-view->m[0][2],fy=-view->m[1][2],fz=-view->m[2][2];
    if(!native_guard_activity_context_valid || choice!=native_guard_activity_context_choice ||
       fx!=native_guard_activity_basis[0] || fy!=native_guard_activity_basis[1] || fz!=native_guard_activity_basis[2]) {
        float norm2=fx*fx+fy*fy+fz*fz;
        if(!(norm2>0)){native_guard_activity_context_valid=0;return 0;}
        float norm=sqrtf(norm2);
        native_guard_activity_forward[0]=fx/norm;native_guard_activity_forward[1]=fy/norm;native_guard_activity_forward[2]=fz/norm;
        native_guard_activity_basis[0]=fx;native_guard_activity_basis[1]=fy;native_guard_activity_basis[2]=fz;
        native_guard_activity_fog_end=native_detail_distance(choice);
        native_guard_activity_context_choice=choice;native_guard_activity_context_valid=1;
    }
    float depth=(chr->prop->pos.x-eye->x)*native_guard_activity_forward[0]+
        (chr->prop->pos.y-eye->y)*native_guard_activity_forward[1]+
        (chr->prop->pos.z-eye->z)*native_guard_activity_forward[2];
    return native_guard_activity_fog_end>0 && depth-radius>=native_guard_activity_fog_end;
}

/* The original renderer can submit a held weapon/hat separately from its
 * guard. Cull the whole attachment tree without hiding independent pickups. */
int native_guard_activity_drawable(PropRecord *prop) {
    for(PropRecord *parent=prop;parent;parent=parent->parent)
        if(parent->type==PROP_TYPE_CHR && native_guard_activity_beyond_fog(parent->chr))return 0;
    return 1;
}

static int native_guard_activity_global(ChrRecord *chr) {
    if(!chr->ailist)return -1;
    for(unsigned i=0;g_GlobalAILists[i].ailist;i++)
        if(g_GlobalAILists[i].ailist==chr->ailist)return g_GlobalAILists[i].ID;
    return -1;
}

static int native_guard_activity_ordinary(int id) {
    return id==GAILIST_STANDARD_GUARD || id==GAILIST_SIMPLE_GUARD ||
        id==GAILIST_SIMPLE_GUARD_DEAF || id==GAILIST_SIMPLE_GUARD_ALARM_RAISER ||
        id==GAILIST_PLAY_IDLE_ANIMATION;
}

static int native_guard_activity_pursuit(int id) {
    return id==GAILIST_RUN_TO_BOND || id==GAILIST_ATTACK_BOND ||
        id==GAILIST_AIM_AT_BOND || id==GAILIST_STARTLE_AND_RUN_TO_BOND ||
        id==GAILIST_TRY_CLONE_SEND_OR_RUN_TO_BOND || id==GAILIST_STANDARD_CLONE ||
        id==GAILIST_PERSISTENTLY_CHASE_AND_ATTACK_BOND || id==GAILIST_DRAW_TT33_AND_ATTCK_BOND;
}

static int native_guard_activity_recent(int stamp) {
    return stamp>0 && g_GlobalTimer>=stamp && g_GlobalTimer-stamp<600;
}

/* A visibility probe, not acquisition: do not mark Bond as seen or alter AI.
 * Test static geometry, doors and opaque objects. Other moving guards must not
 * put a whole guard to sleep just by briefly standing across its sightline. */
static int native_guard_activity_clear(ChrRecord *chr,PropRecord *bond) {
    PropRecord *prop=chr->prop;
    if(!prop->stan || !bond->stan)return 1;
    StandTile *stan=prop->stan;
    unsigned saved_hidden=chr->hidden;
    chrSetMoving(chr,FALSE);
    bondviewUpdateGuardTankFlagsRelated(bond,0);
    float height=chr->chrheight-20.f;
    int clear=stanTestLineUnobstructed(&stan,prop->pos.x,prop->pos.z,
        bond->pos.x,bond->pos.z,
        CDTYPE_OBJS|CDTYPE_DOORS|CDTYPE_PATHBLOCKER|CDTYPE_AIOPAQUE,
        height,height,0.f,1.f) && stan==bond->stan;
    bondviewUpdateGuardTankFlagsRelated(bond,1);
    chr->hidden=saved_hidden;
    native_guard_activity_ray_count++;
    return clear;
}

int native_guard_activity_awake(ChrRecord *chr) {
    int now=g_GlobalTimer;
    if(now<native_guard_activity_frame)native_guard_activity_reset();
    if(now!=native_guard_activity_frame){native_guard_activity_frame=now;native_guard_activity_probes=0;}
    struct NativeGuardActivity *s=native_guard_activity_slot(chr);
    if(!s || !chr->model || !chr->prop || chr->prop->type!=PROP_TYPE_CHR ||
       lvlGetCurrentStageToLoad()==LEVELID_TITLE || getPlayerCount()!=1 || g_ClockTimer<=0)return 1;
    /* Fog suspension precedes every alert/action/script wake exception. */
    if(native_guard_activity_beyond_fog(chr)) {
        if(!s->fog_sleep || s->decision_tick!=now)native_guard_activity_fog_count++;
        s->fog_sleep=1;s->awake=0;s->decision_tick=now;return 0;
    }
    if(s->fog_sleep) {
        s->fog_sleep=0;s->decision_tick=s->ai_tick=-1;
        s->probe_tick=now;s->clear=1;
    }
    /* Damage, hearing and script changes can happen after an earlier call in
     * this update. Evaluate wake signals before consulting the cached verdict. */
    int id=native_guard_activity_global(chr);
    int interrupted=(chr->hidden&(CHRHIDDEN_REMOVE|CHRHIDDEN_ALERT_GUARD_RELATED|
          CHRHIDDEN_FIRE_WEAPON_LEFT|CHRHIDDEN_FIRE_WEAPON_RIGHT)) ||
        (chr->chrflags&(CHRFLAG_INIT|CHRFLAG_NEAR_MISS|CHRFLAG_00040000)) ||
        (chr->hidden&CHRHIDDEN_DROP_HELD_ITEMS) ||
        chr->damage!=s->damage;
    if(chr->damage!=s->damage)s->ai_tick=-1;
    s->damage=chr->damage;
    int ordinary=native_guard_activity_ordinary(id);
    if(interrupted || (!ordinary && !native_guard_activity_pursuit(id)) ||
       !isGlobalAIListID(chr->aireturnlist) ||
       chr->actiontype==ACT_INIT || chr->actiontype==ACT_DIE ||
       chr->actiontype==ACT_DEAD || chr->actiontype==ACT_ARGH ||
       chr->actiontype==ACT_PREARGH || chr->actiontype==ACT_STARTALARM ||
       chr->actiontype==ACT_THROWGRENADE ||
       (chr->actiontype==ACT_ANIM && id!=GAILIST_PLAY_IDLE_ANIMATION)) {
        s->awake=1;s->decision_tick=now;return 1;
    }
    int engaged=!ordinary || native_guard_activity_recent(chr->lastseetarget60) ||
        native_guard_activity_recent(chr->lastheartarget60) ||
        (chr->actiontype!=ACT_STAND && chr->actiontype!=ACT_KNEEL &&
         chr->actiontype!=ACT_PATROL && chr->actiontype!=ACT_ANIM);
    if(s->decision_tick==now){
        if(!s->fog_sleep && engaged)s->awake=1;
        return s->awake;
    }
    s->decision_tick=now;
    PropRecord *bond=getCurrentPlayerProp();
    if(!bond)return s->awake=1;
    float dx=chr->prop->pos.x-bond->pos.x,dy=chr->prop->pos.y-bond->pos.y;
    float dz=chr->prop->pos.z-bond->pos.z;
    float distance2=dx*dx+dy*dy+dz*dz;
    /* Close guards always wake, even before a scheduled visibility probe. */
    if(distance2<=200.f*200.f){s->awake=1;s->fog_sleep=0;return 1;}
    /* Visibility can never override hard fog suspension. Inside the fog,
     * retain whole-model visibility protection and ordinary LOS policy. */
    float radius=getinstsize(chr->model);
    if(posIsOnScreen(chr->prop,&chr->prop->pos,radius,1))return s->awake=1;
    if(engaged)return s->awake=1;
    if(native_setting_guard_visibility())return s->awake=0;
    if(now>=s->probe_tick && native_guard_activity_probes<2) {
        s->clear=native_guard_activity_clear(chr,bond);
        s->probe_tick=now+15;native_guard_activity_probes++;
    }
    s->awake=s->clear;
    if(s->awake)native_guard_activity_awake_count++;else native_guard_activity_sleep_count++;
    return s->awake;
}

/* Opt-in perception gate. This never clears render visibility or sleeps a
 * model based on one ray. Mission scripts and alerted/pursuing guards retain
 * the original perception so scripted actions and pursuit cannot stall. */
int native_guard_activity_sight_allowed(ChrRecord *chr) {
    if(native_guard_activity_beyond_fog(chr))return 0;
    if(!native_setting_guard_visibility() || !chr || !chr->prop || !chr->model ||
       lvlGetCurrentStageToLoad()==LEVELID_TITLE || getPlayerCount()!=1 || g_ClockTimer<=0)return 1;
    struct NativeGuardActivity *s=native_guard_activity_slot(chr);
    int id=native_guard_activity_global(chr);
    if(!s || !native_guard_activity_ordinary(id) || !isGlobalAIListID(chr->aireturnlist) ||
       native_guard_activity_recent(chr->lastseetarget60) ||
       native_guard_activity_recent(chr->lastheartarget60) ||
       chr->damage!=s->damage ||
       (chr->chrflags&(CHRFLAG_INIT|CHRFLAG_NEAR_MISS)) ||
       (chr->hidden&CHRHIDDEN_ALERT_GUARD_RELATED))return 1;
    if(!posIsOnScreen(chr->prop,&chr->prop->pos,getinstsize(chr->model),1))return 0;
    int now=g_GlobalTimer;
    if(now<native_guard_activity_frame)native_guard_activity_reset();
    if(now!=native_guard_activity_frame){native_guard_activity_frame=now;native_guard_activity_probes=0;}
    if(now>=s->probe_tick && native_guard_activity_probes<2) {
        PropRecord *bond=getCurrentPlayerProp();
        if(!bond)return 1;
        s->clear=native_guard_activity_clear(chr,bond);
        s->probe_tick=now+15;native_guard_activity_probes++;
    }
    return s->clear;
}

int native_guard_activity_ai_due(ChrRecord *chr) {
    if(native_guard_activity_beyond_fog(chr))return 0;
    struct NativeGuardActivity *s=native_guard_activity_slot(chr);
    /* Local mission scripts and non-slot background actors retain their rate. */
    if(!s || !chr->prop || chr->prop->type!=PROP_TYPE_CHR ||
       lvlGetCurrentStageToLoad()==LEVELID_TITLE || getPlayerCount()!=1 ||
       native_guard_activity_global(chr)<0 ||
       !isGlobalAIListID(chr->aireturnlist))return 1;
    int now=g_GlobalTimer;
    int hz=native_setting_guard_ai_hz();
    int interval=hz==5?12:hz==15?4:2;
    if(s->ai_tick<0 || now<s->ai_tick || now-s->ai_tick>=interval ||
       (chr->chrflags&CHRFLAG_NEAR_MISS)) {
        s->ai_tick=now;native_guard_activity_ai_run++;return 1;
    }
    native_guard_activity_ai_skip++;return 0;
}

void native_guard_activity_sleep(ChrRecord *chr) {
    if(native_guard_activity_beyond_fog(chr))return;
    struct NativeGuardActivity *s=native_guard_activity_slot(chr);
    if(!s || s->sleep_tick==g_GlobalTimer)return;
    s->sleep_tick=g_GlobalTimer;
    if((chr->hidden&CHRHIDDEN_TIMER_ACTIVE) && g_ClockTimer>0)chr->timer60+=g_ClockTimer;
    int sleep=(int)chr->sleep-g_ClockTimer;
    chr->sleep=sleep>0?sleep:0;
}

void native_guard_activity_report(void) {
    if(g_GlobalTimer-native_guard_activity_report_tick<300)return;
    native_guard_activity_report_tick=g_GlobalTimer;
    sysLogPrintf(LOG_NOTE,"Guard activity: awake=%u asleep=%u beyondFog=%u visibilityRays=%u AIrun=%u AIskipped=%u (AI cap %dHz, visibility cap 4Hz, LOS gate=%d)",
        native_guard_activity_awake_count,native_guard_activity_sleep_count,
        native_guard_activity_fog_count,native_guard_activity_ray_count,
        native_guard_activity_ai_run,native_guard_activity_ai_skip,
        native_setting_guard_ai_hz(),native_setting_guard_visibility());
    native_guard_activity_awake_count=native_guard_activity_sleep_count=0;
    native_guard_activity_ai_run=native_guard_activity_ai_skip=0;
    native_guard_activity_ray_count=native_guard_activity_fog_count=0;
}
