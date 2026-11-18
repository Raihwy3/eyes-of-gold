#include <ultra64.h>
#include "game/player.h"
#include "game/bondview.h"
#include "game/propobj.h"
#include "game/stan.h"
#include "native_settings.h"
#include "native_pickup.h"
#include <stdio.h>
#include <assert.h>
#include "native_test_assert.h"
static struct player player;
struct player *g_CurrentPlayer=&player;
static PropRecord player_prop,item_prop;
static ObjectRecord item;
static float pitch;
static int style,wall,line_mismatch,collected;
int native_setting_style(void){return style;}
f32 bondviewGetPlayerPitchRadians(void){return pitch;}
PropRecord *getCurrentPlayerProp(void){return &player_prop;}
TICKOP propPickupByPlayer(PropRecord *p,bool show){assert(p==&item_prop && show);collected++;return TICKOP_GIVETOPLAYER;}
s32 stanTestLineUnobstructed(StandTile **tile,f32 x,f32 z,f32 tx,f32 tz,int types,f32 h,f32 a,f32 b,f32 c) {
 assert(x==player_prop.pos.x && z==player_prop.pos.z && tx==item_prop.pos.x && tz==item_prop.pos.z);
 assert(types==2 && h==30 && a==30 && b==0 && c==1);
 if(!line_mismatch)*tile=item_prop.stan;
 return !wall;
}
static TICKOP original_pickup_position(PropRecord *prop) {
 ObjectRecord *obj=prop->obj;
#include "native-pickup-position.h"
int main(void) {
 item_prop.obj=&item;
 player_prop.stan=(StandTile *)0x1000;item_prop.stan=(StandTile *)0x2000;
 player.magnetattracttime=-1;item.runtime_pos.x=100;
 for(style=0;style<10;style++) {
  pitch=-1.f;collected=0;
  TICKOP op=original_pickup_position(&item_prop);
  assert(collected==(style>=8));assert(op==(style>=8?TICKOP_GIVETOPLAYER:TICKOP_NONE));
  pitch=0;collected=0;assert(original_pickup_position(&item_prop)==TICKOP_GIVETOPLAYER && collected==1);
 }
 style=8;pitch=-1;
 item.runtime_pos.x=101;assert(original_pickup_position(&item_prop)==TICKOP_NONE);
 item.runtime_pos.x=100;item.runtime_pos.y=201;assert(original_pickup_position(&item_prop)==TICKOP_NONE);
 item.runtime_pos.y=0;wall=1;assert(original_pickup_position(&item_prop)==TICKOP_NONE);wall=0;
 line_mismatch=1;assert(original_pickup_position(&item_prop)==TICKOP_NONE);line_mismatch=0;
 for(style=0;style<10;style++) {
  player.magnetattracttime=60;item.runtime_pos.x=350;item.runtime_pos.y=500;
  assert(original_pickup_position(&item_prop)==TICKOP_GIVETOPLAYER);
  item.runtime_pos.x=351;assert(original_pickup_position(&item_prop)==TICKOP_NONE);
 }
 puts("PASS actual pickup position consumer: looking-down modern collection, original-layout pitch gate, original 100/350 radii, vertical limits, walls and STAN endpoint checks");
}
