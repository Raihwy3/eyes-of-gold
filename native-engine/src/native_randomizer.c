/* Optional setup-time shuffling. Original asset descriptions come from ROM-backed
 * engine tables; this module supplies selection rules, never game asset data. */
#include <ultra64.h>
#include "bondconstants.h"
#include "bondtypes.h"
#include "game/prop.h"
#include "game/front.h"
#include "game/player.h"
#include "game/loadobjectmodel.h"
#include "game/mp_weapon.h"
#include "native_settings.h"
#include "native_randomizer.h"
extern struct stagesetup g_CurrentSetup;
extern s32 get_is_ramrom_flag(void);
extern s32 get_recording_ramrom_flag(void);
#define MAX_RECORDS 4096
static PropDefHeaderRecord *records[MAX_RECORDS];
static unsigned char protected_record[MAX_RECORDS],ordinary_guard[MAX_RECORDS];
static unsigned story_seed;
static int story_active;
static unsigned mix(unsigned n) {
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;return n^(n>>16);
}
static int firearm(int item){return item>=ITEM_WPPK && item<=ITEM_RUGER;}
static int bullets(int ammo){return ammo==AMMO_9MM || ammo==AMMO_9MM_2 || ammo==AMMO_RIFLE || ammo==AMMO_SHOTGUN || ammo==AMMO_MAGNUM;}
static int guard_body(int body) {
    switch(body) {
    case BODY_Jungle_Commando:case BODY_St_Petersburg_Guard:
    case BODY_Russian_Soldier:case BODY_Russian_Infantry:
    case BODY_Janus_Special_Forces:case BODY_Janus_Marine:
    case BODY_Russian_Commandant:case BODY_Naval_Officer:
    case BODY_Siberian_Special_Forces:case BODY_Siberian_Guard_2:
    case BODY_Arctic_Commando:case BODY_Moonraker_Elite_1_Male:return 1;
    default:return 0;
    }
}
/* Appearance only: the constructor retains the original body/head IDs for
 * gameplay checks, original scripts, weapons, allegiance and invulnerability. */
static int story_body(int body,int head) {
    switch(body) {
    case BODY_Boris:case BODY_Ourumov:case BODY_Trevelyan_Janus:
    case BODY_Trevelyan_006:case BODY_Valentin_:case BODY_Xenia:
    case BODY_Baron_Samedi:case BODY_Jaws:case BODY_Mayday:
    case BODY_Oddjob:case BODY_Natalya_Skirt:case BODY_Natalya_Jungle_Fatigues:
    case BODY_Rosika:return 1;
    default:return head==BODY_Male_Mishkin || head==BODY_Male_Dave_Dr_Doak || head==BODY_Female_Marion_Rosika;
    }
}
void native_randomizer_story_appearance(int character,int *body,int *head) {
    if(!story_active || !story_body(*body,*head))return;
    /* These story models have integrated heads. Two resident choices per
     * stage keep this from loading the entire cast on Old 3DS. */
    int cast[]={BODY_Boris,BODY_Ourumov,BODY_Trevelyan_Janus,BODY_Trevelyan_006,BODY_Valentin_,BODY_Xenia,BODY_Baron_Samedi,BODY_Jaws,BODY_Mayday,BODY_Oddjob,BODY_Natalya_Skirt,BODY_Natalya_Jungle_Fatigues};
    int count=sizeof cast/sizeof *cast;
    int first=mix(story_seed^0x57a71u)%count;
    int second=(first+1+mix(story_seed^0x57a72u)%(count-1))%count;
    int chosen=(mix(story_seed^(unsigned)character)&1)?first:second;
    if(cast[chosen]==*body)chosen=chosen==first?second:first;
    *body=cast[chosen];*head=-1;
}
static void protect(int index,int count){if(index>=0 && index<count)protected_record[index]=1;}
static int ordinary_ammo(MultiAmmoCrateRecord *crate) {
    for(int i=0;i<AMMOTYPE_GLOBAL_MAX;i++)if(crate->slots[i].quantity && !bullets(i+1))return 0;
    return 1;
}
void native_randomizer_setup(int stage) {
    story_active=0;
    if(!native_setting_randomiser() || getPlayerCount()!=1 || gamemode!=GAMEMODE_SOLO || get_is_ramrom_flag() || get_recording_ramrom_flag())return;
    PropDefHeaderRecord *p=g_CurrentSetup.propDefs;
    int count=0;
    while(p && p->type!=PROPDEF_END) {
        if(count==MAX_RECORDS)return; /* Fail unchanged, rather than partly shuffle. */
        int words=sizepropdef(p);if(words<=0)return;
        records[count]=p;protected_record[count]=ordinary_guard[count]=0;count++;
        p=(void *)((u32 *)p+words);
    }
    for(int i=0;i<count;i++) {
        p=records[i];
        if(p->type==PROPDEF_TAG)protect(i+((TagObjectRecord *)p)->OffsetToObj,count);
        if(p->type==PROPDEF_LINK || p->type==PROPDEF_SWITCH) {
            LinkRecord *link=(void *)p;protect(i+link->Index1,count);protect(i+link->Index2,count);
        }
        if(p->type==PROPDEF_SAFE_ITEM) {
            SafeObjectRecord *link=(void *)p;protect(i+link->Index1,count);protect(i+link->Index2,count);protect(i+link->Index3,count);
        }
    }
    unsigned seed=mix((unsigned)native_setting_randomiser_seed() ^ ((unsigned)stage*0x9e3779b9u));
    story_seed=seed;story_active=1;
    /* Cap additional resident models: four firearm types and two male bodies.
     * Matching existing male heads remain valid. Story/civilian bodies stay put. */
    struct s_mp_weapon_set *sets[]={mp_weapon_set_pistols,mp_weapon_set_auto,mp_weapon_set_power,mp_weapon_set_sniper};
    struct s_mp_weapon_set *candidates[32],*guns[4];int gun_count=0;
    for(int s=0;s<4;s++)for(int i=0;i<8;i++) {
        struct s_mp_weapon_set *w=&sets[s][i];int duplicate=0;
        if(!firearm(w->itemID) || !w->allowpickup || !bullets(w->ammotype))continue;
        for(int j=0;j<gun_count;j++)if(candidates[j]->itemID==w->itemID)duplicate=1;
        if(!duplicate)candidates[gun_count++]=w;
    }
    if(!gun_count)return;
    for(int i=gun_count-1;i>0;i--){int j=mix(seed+i)%(i+1);struct s_mp_weapon_set *w=candidates[i];candidates[i]=candidates[j];candidates[j]=w;}
    int pool=gun_count<4?gun_count:4;for(int i=0;i<pool;i++)guns[i]=candidates[i];
    int bodies[]={BODY_Jungle_Commando,BODY_St_Petersburg_Guard,BODY_Russian_Soldier,BODY_Russian_Infantry,BODY_Janus_Special_Forces,BODY_Janus_Marine,BODY_Russian_Commandant,BODY_Naval_Officer,BODY_Siberian_Special_Forces,BODY_Siberian_Guard_2,BODY_Arctic_Commando,BODY_Moonraker_Elite_1_Male};
    int body_count=sizeof bodies/sizeof *bodies;
    for(int i=body_count-1;i>0;i--){int j=mix(seed^0xa03du^i)%(i+1);int b=bodies[i];bodies[i]=bodies[j];bodies[j]=b;}
    for(int i=0;i<count;i++)if(records[i]->type==PROPDEF_GUARD && !protected_record[i]) {
        GuardRecord *g=(void *)records[i];
        if(guard_body(g->BodyID) && !(g->bitflags&GUARD_SETUP_FLAG_CHR_INVINCIBLE)) {
            ordinary_guard[i]=1;g->BodyID=bodies[mix(seed^0x12345u^i)%2];
        }
    }
    for(int i=0;i<count;i++)if(!protected_record[i] && records[i]->type==PROPDEF_COLLECTABLE) {
        WeaponObjRecord *w=(void *)records[i];if(!firearm(w->weaponnum))continue;
        if(w->flags&PROPFLAG_INSIDEANOTHEROBJ)continue;
        if(w->flags&PROPFLAG_ASSIGNEDTOCHR) {
            int allowed=0;
            for(int j=0;j<count;j++)if(ordinary_guard[j] && ((GuardRecord *)records[j])->chrnum==w->pad){allowed=1;break;}
            if(!allowed)continue;
        }
        struct s_mp_weapon_set *choice=guns[mix(seed^0x77777u^i)%pool];
        w->weaponnum=choice->itemID;w->obj=choice->propID;w->extrascale=(u16)(choice->size*256.f);
    }
    /* Shuffle whole ordinary ammo payloads, retaining each slot's correct model
     * and quantity. Crates with explosives or mission supplies are left alone. */
    int seen=0;
    for(int i=0;i<count;i++)if(!protected_record[i] && records[i]->type==PROPDEF_AMMO && ordinary_ammo((void *)records[i])) {
        int target=mix(seed^0xaabbccu^i)%(++seen);
        for(int j=0;j<=i;j++)if(!protected_record[j] && records[j]->type==PROPDEF_AMMO && ordinary_ammo((void *)records[j]) && target--==0) {
            MultiAmmoCrateRecord *a=(void *)records[i],*b=(void *)records[j];
            for(int k=0;k<AMMOTYPE_GLOBAL_MAX;k++){struct multiammocrateslot slot=a->slots[k];a->slots[k]=b->slots[k];b->slots[k]=slot;}break;
        }
    }
    /* Existing gun loadouts become one matching weapon/ammo type. Gadget and
     * explosive records are untouched; Subsistence filters these guns later. */
    struct s_mp_weapon_set *start=guns[mix(seed^0xfeedu)%pool];
    s32 *intro=g_CurrentSetup.intro;
    static const unsigned char intro_words[]={3,4,4,8,2,2,10,3,2};
    while(intro && intro[0]!=INTROTYPE_END) {
        int type=intro[0];if(type<0 || type>=9)break;
        if(type==INTROTYPE_ITEM && intro[3]==0 && firearm(intro[1])){intro[1]=start->itemID;intro[2]=-1;}
        if(type==INTROTYPE_AMMO && intro[3]==0 && bullets(intro[1])){intro[1]=start->ammotype;intro[2]=start->ammoamount;}
        intro+=intro_words[type];
    }
}
