#include <ultra64.h>
#include "bondconstants.h"
#include "bondtypes.h"
#include "game/mp_weapon.h"
#include "native_settings.h"
#include "native_test_assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* WEAPON_TABLES */
static int players=1,demo,recording;
GAMEMODE gamemode=GAMEMODE_SOLO;
struct stagesetup g_CurrentSetup;
s32 getPlayerCount(void){return players;}
s32 get_is_ramrom_flag(void){return demo;}
s32 get_recording_ramrom_flag(void){return recording;}
/* Real typed fields and alignment, with a uniform stride for fixture traversal. */
typedef union {PropDefHeaderRecord header;GuardRecord guard;WeaponObjRecord weapon;MultiAmmoCrateRecord ammo;TagObjectRecord tag;LinkRecord link;SafeObjectRecord safe;unsigned char padding[512];} Record;
static Record r[4097],before[4097],after[4097];static int count,changed;
s32 sizepropdef(PropDefHeaderRecord *p){(void)p;return sizeof(Record)/4;}
#include "../src/native_randomizer.c"
static void check_stage(int stage,s32 *intro,size_t bytes){
 s32 saved[2048],result[2048];assert(bytes<=sizeof saved);memcpy(saved,intro,bytes);
 memcpy(before,r,sizeof r);g_CurrentSetup.propDefs=(void *)r;g_CurrentSetup.intro=intro;
 native_setting_set_randomiser(0);native_randomizer_setup(stage);assert(!memcmp(before,r,sizeof r) && !memcmp(saved,intro,bytes));
 {int body=BODY_Boris,head=-1;native_randomizer_story_appearance(1,&body,&head);assert(body==BODY_Boris && head==-1);}
 native_setting_set_randomiser(1);
 for(int mode=0;mode<4;mode++){
  players=mode==0?2:1;demo=mode==1;recording=mode==2;gamemode=mode==3?GAMEMODE_MULTI:GAMEMODE_SOLO;
  native_randomizer_setup(stage);assert(!memcmp(before,r,sizeof r) && !memcmp(saved,intro,bytes));
 {int body=BODY_Boris,head=-1;native_randomizer_story_appearance(1,&body,&head);assert(body==BODY_Boris && head==-1);}
 }
 players=1;demo=recording=0;gamemode=GAMEMODE_SOLO;
 native_setting_set_randomiser_seed(121314);native_randomizer_setup(stage); memcpy(after,r,sizeof r);memcpy(result,intro,bytes);
 int cast_bodies[]={BODY_Boris,BODY_Ourumov,BODY_Trevelyan_Janus,BODY_Trevelyan_006,BODY_Valentin_,BODY_Xenia,BODY_Baron_Samedi,BODY_Jaws,BODY_Mayday,BODY_Oddjob,BODY_Natalya_Skirt,BODY_Natalya_Jungle_Fatigues,BODY_Rosika};
 int seen[256]={0},distinct=0;
 for(unsigned k=0;k<sizeof cast_bodies/sizeof *cast_bodies;k++){
  int body=cast_bodies[k],head=-1,again=body,againhead=head;
  native_randomizer_story_appearance(k,&body,&head);assert(body!=cast_bodies[k] && head==-1);
  native_randomizer_story_appearance(k,&again,&againhead);assert(body==again && head==againhead);
  if(!seen[body]++){distinct++;}assert(distinct<=2);
 }
 int body=BODY_Scientist_1_Male,head=BODY_Male_Dave_Dr_Doak;
 native_randomizer_story_appearance(2,&body,&head);assert(body!=BODY_Scientist_1_Male && head==-1);

 for(int i=0;i<count;i++){
  assert(r[i].header.type==before[i].header.type);
  if(protected_record[i])assert(!memcmp(&r[i],&before[i],sizeof r[i]));
  else if(r[i].header.type==PROPDEF_GUARD){
   GuardRecord copy=r[i].guard;copy.BodyID=before[i].guard.BodyID;assert(!memcmp(&copy,&before[i].guard,sizeof copy));
   if(!guard_body(before[i].guard.BodyID) || (before[i].guard.bitflags&GUARD_SETUP_FLAG_CHR_INVINCIBLE))assert(r[i].guard.BodyID==before[i].guard.BodyID);
  }else if(r[i].header.type==PROPDEF_COLLECTABLE){
   WeaponObjRecord copy=r[i].weapon;copy.weaponnum=before[i].weapon.weaponnum;copy.obj=before[i].weapon.obj;copy.extrascale=before[i].weapon.extrascale;
   assert(!memcmp(&copy,&before[i].weapon,sizeof copy));
   if(!firearm(before[i].weapon.weaponnum))assert(!memcmp(&r[i],&before[i],sizeof r[i]));
   if(r[i].weapon.weaponnum!=before[i].weapon.weaponnum)assert(firearm(r[i].weapon.weaponnum));
  }else if(r[i].header.type==PROPDEF_AMMO){
   MultiAmmoCrateRecord copy=r[i].ammo;memcpy(copy.slots,before[i].ammo.slots,sizeof copy.slots);assert(!memcmp(&copy,&before[i].ammo,sizeof copy));
   if(!ordinary_ammo(&before[i].ammo))assert(!memcmp(&r[i],&before[i],sizeof r[i]));
  }else assert(!memcmp(&r[i],&before[i],sizeof r[i]));
 }
 static const int strides[]={3,4,4,8,2,2,10,3,2};
 for(unsigned at=0;at<bytes/4 && saved[at]!=9;at+=strides[saved[at]]){
  int type=saved[at];
  if(!((type==INTROTYPE_ITEM && saved[at+3]==0 && firearm(saved[at+1])) || (type==INTROTYPE_AMMO && saved[at+3]==0 && bullets(saved[at+1]))))assert(!memcmp(saved+at,intro+at,strides[type]*4));
 }
 memcpy(r,before,sizeof r);memcpy(intro,saved,bytes);native_randomizer_setup(stage);assert(!memcmp(after,r,sizeof r) && !memcmp(result,intro,bytes));
 memcpy(r,before,sizeof r);memcpy(intro,saved,bytes);native_setting_set_randomiser_seed(984321);native_randomizer_setup(stage);if(memcmp(after,r,sizeof r) || memcmp(result,intro,bytes))changed++;
}
/* FIXTURES */
int main(void){
 char directory[]="/tmp/eog-randomizer-XXXXXX";assert(mkdtemp(directory));native_settings_init(directory);
 assert(!native_setting_randomiser() && native_setting_randomiser_seed()==1);
 native_setting_set_style(8);native_setting_set_aspect(1);native_setting_set_randomiser(1);native_setting_set_randomiser_seed(123456);native_settings_init(directory);
 assert(native_setting_randomiser() && native_setting_randomiser_seed()==123456);
 native_setting_set_randomiser_seed(0);native_setting_set_randomiser_seed(1000000);assert(native_setting_randomiser_seed()==123456);
 /* RUN */
 assert(changed==20);
 char path[512];snprintf(path,sizeof path,"%s/native-options.ini",directory);unlink(path);rmdir(directory);
 puts("PASS Randomiser: 20 retail setups, deterministic seeds, different seeds, protected objects/gadgets, story appearance swaps with two-model cap, matching ammo records, demo/multiplayer exclusions and persistence");
}
