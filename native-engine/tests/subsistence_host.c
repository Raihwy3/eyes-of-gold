#include <ultra64.h>
#include "bondconstants.h"
#include "bondtypes.h"
#include "native_settings.h"
#include "native_subsistence.h"
#include "native_cheat_layout.h"
#include "native_test_assert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int players=1,stage,demo,recording,slot;
GAMEMODE gamemode=GAMEMODE_SOLO;
s32 get_is_ramrom_flag(void){return demo;}
s32 get_recording_ramrom_flag(void){return recording;}
int getPlayerCount(void){return players;}
static int bossGetStageNum(void){return stage;}
u32 check_ramrom_flags(void){return slot;}
static int items[128],ammo[AMMOTYPE_MAX],starting_weapon[2];
static int bondinvAddInvItem(int item){assert(item>=0 && item<128);items[item]=1;return 1;}
static int bondinvAddDoublesInvItem(int right,int left){bondinvAddInvItem(right);bondinvAddInvItem(left);return 1;}
u32 weaponLoadProjectileModels(int item){(void)item;return 0;}
static void give_cur_player_ammo(int type,int count){assert(type>=0 && type<AMMOTYPE_MAX);ammo[type]+=count;}
static void add_ammo_to_weapon(int item,int count){(void)item;(void)count;}
static void apply_intro(s32 *records){
 struct SetupIntroEmpty *intro_record=(void *)records;struct SetupIntroItem *intro_item;
 int set_starting_weapon=0;starting_weapon[0]=starting_weapon[1]=ITEM_UNARMED;
 memset(items,0,sizeof items);memset(ammo,0,sizeof ammo);
 while(intro_record->type!=INTROTYPE_END)switch(intro_record->type){
 /* INTRO_CASES */
 default:abort();
 }
 /* INTRO_FINISH */
}
static int tab_start_selected,tab_next_selected,tab_prev_selected,tab_prev_highlight,tab_next_highlight,tab_start_highlight;
static int totalunlockedcheats,g_AppendCheatSinglePlayer,g_AppendCheatMultiPlayer;
static u8 cheat_available[CHEAT_MAX],g_CheatActivated[CHEAT_MAX];
static u32 arrayUnlockedCheats[CHEAT_MAX];
static int cheatCheckIfMPCheat(int id){(void)id;return 0;}
static char *cheatGetMenuTextPointer(int id){assert(id>0 && id<CHEAT_INVALID);return "Retail cheat";}
static void load_walletbond(void){}
/* CHEAT_HELPERS */
/* RETAIL_FIXTURES */
int main(void){
 /* Regression: even a full retail list plus the three custom entries must
  * remain selectable, with draw coordinates and touch focus agreeing. */
 for(int count=3;count<=31;count++)for(int tag=0;tag<count;tag++){
  int split=native_cheat_split(count),row=tag<split?tag:tag-split;
  int y=native_cheat_row_y(row,count),x=tag<split?100:240;
  assert(y>=53 && y+14<=287 && native_cheat_pitch(count)>=14);
  assert(native_cheat_hit(count,x,y+5)==tag);
 }
 unsetenv("GE_STARTWEAPON");
 char directory[]="/tmp/eog-subsistence-XXXXXX";assert(mkdtemp(directory));
 native_settings_init(directory);assert(!native_setting_subsistence());
 /* Save a valid baseline configuration before testing persistence. */
 native_setting_set_style(8);native_setting_set_aspect(1);
 native_setting_set_subsistence(1);native_settings_init(directory);assert(native_setting_subsistence());
 cheat_available[1]=cheat_available[2]=1;
 init_menu15_cheat();assert(totalunlockedcheats==5);
 assert(arrayUnlockedCheats[0]==1 && arrayUnlockedCheats[1]==2);
 assert(arrayUnlockedCheats[2]==0 && arrayUnlockedCheats[3]==CHEAT_INVALID+1 && arrayUnlockedCheats[4]==CHEAT_INVALID+2);
 assert(!strcmp((char *)native_cheat_label(0),"Subsistence"));assert(native_cheat_active(0));
 assert(!native_cheat_active(CHEAT_INVALID+1));
 native_cheat_toggle(CHEAT_INVALID+1);assert(native_setting_randomiser());
 assert(!strcmp((char *)native_cheat_label(CHEAT_INVALID+1),"Randomiser"));
 int oldseed=native_setting_randomiser_seed();native_cheat_toggle(CHEAT_INVALID+2);assert(native_setting_randomiser_seed()!=oldseed);
 assert(strlen((char *)native_cheat_value(CHEAT_INVALID+2,(u8 *)"Off"))==6);
 native_cheat_toggle(CHEAT_INVALID+1);assert(!native_setting_randomiser());
 native_custom_cheats_sync();assert(g_AppendCheatSinglePlayer && !g_AppendCheatMultiPlayer);
 native_cheat_toggle(CHEAT_ALLGUNS);assert(!native_setting_subsistence() && g_CheatActivated[CHEAT_ALLGUNS]);
 native_cheat_toggle(0);assert(native_setting_subsistence() && !g_CheatActivated[CHEAT_ALLGUNS]);
 for(unsigned n=0;n<sizeof retail/sizeof *retail;n++){
  stage=retail[n].stage;
  for(int enabled=0;enabled<2;enabled++){
   native_setting_set_subsistence(enabled);apply_intro(retail[n].records);
   assert(items[ITEM_FIST]);
   if(enabled){
    assert(starting_weapon[GUNRIGHT]==ITEM_FIST && starting_weapon[GUNLEFT]==ITEM_UNARMED);
    for(int i=0;i<128;i++)if(!native_subsistence_item(i,stage))assert(!items[i]);
    for(int i=1;i<AMMOTYPE_MAX;i++)if(!native_subsistence_ammo(i,stage))assert(!ammo[i]);
   }
   /* Every authored starting gadget and its charges survives unchanged. */
   for(s32 *p=retail[n].records;*p!=9;p+=4)if(p[3]==0){
    if(p[0]==1 && (!enabled || native_subsistence_item(p[1],stage)))assert(items[p[1]]);
    if(p[0]==2 && (!enabled || native_subsistence_ammo(p[1],stage)))assert(ammo[p[1]]>=p[2]);
   }
  }
 }
 assert(native_subsistence_item(ITEM_REMOTEMINE,LEVELID_FACILITY));
 assert(native_subsistence_ammo(AMMO_REMOTEMINE,LEVELID_SURFACE2));
 assert(native_subsistence_item(ITEM_WATCHLASER,LEVELID_TRAIN));
 native_setting_set_subsistence(1);
 assert(!native_subsistence_enabled(2,GAMEMODE_SOLO,0));
 assert(!native_subsistence_enabled(1,GAMEMODE_MULTI,0));
 assert(!native_subsistence_enabled(1,GAMEMODE_SOLO,1));
 demo=1;slot=0;stage=LEVELID_DAM;apply_intro(retail[0].records);assert(items[ITEM_WPPKSIL]);demo=0;
 players=2;apply_intro(retail[0].records);assert(items[ITEM_WPPKSIL]);players=1;
 native_cheat_toggle(0);assert(!native_setting_subsistence());native_settings_init(directory);assert(!native_setting_subsistence());
 char path[512];snprintf(path,sizeof path,"%s/native-options.ini",directory);unlink(path);rmdir(directory);
 puts("PASS Subsistence: actual intro branches across 20 ROM loadouts, required gadgets/charges, fist selection, demo/multiplayer exclusion, cheat toggle/conflicts and persistence");
}
