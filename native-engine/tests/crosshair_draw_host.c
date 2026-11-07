/* Exercise the generated sight renderer: persistent visibility must never
 * replace or mutate the actual free-aim position. */
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_settings.h"
#include "native_crosshair_opacity.h"
#define PORT 1
#define VERSION_US 1
#define FALSE 0
#define GUNSIGHTREASON_NOTAIMING 2
#define G_TX_NOMIRROR 0
#define G_TX_CLAMP 2
#define SCREEN_RATIO_16_9 1
#define NATIVE_RETICLE_SCOPE 0x40000
#define gDma0p(p,a,b,c) ((void)(p),(void)(a),(void)(b),(void)(c))
typedef int32_t s32;typedef float f32;
typedef struct {uint32_t words[2];} Gfx;
struct sImageTableEntry {int level,flagsS,flagsT;};
struct sImageTableEntry image,*crosshairimage=&image,*betacrosshairimage;
static struct {int insightaimmode,gunsightmode,mpmenuon;struct {float f[3];} crosshair_angle;} player;
static __typeof__(player) *g_CurrentPlayer=&player;
s32 portCrosshairHide;
static unsigned draws,custom_draws,sprite_alpha;static float drawn[2];
void portCrosshairTint(s32 *r,s32 *g,s32 *b){*r=*g=*b=255;}
void portCrosshairApplyTintCombine(Gfx *p){(void)p;}
int portCrosshairStyle(void){return 0;}
f32 portCrosshairScale(void){return 1;}
f32 portNativeAspect(void){return 4.f/3;}
int get_screen_ratio(void){return 0;}
void texSelect(Gfx **g,struct sImageTableEntry *i,int a,int b,int c){assert(i);(void)a;(void)b;(void)c;(*g)++;}
void display_image_at_position(Gfx **g,float (*xy)[2],float (*half)[2],...){
 va_list args;va_start(args,half);for(int i=0;i<8;i++)(void)va_arg(args,int);sprite_alpha=va_arg(args,unsigned);va_end(args);
 assert((*half)[0]>0 && (*half)[1]>0);draws++;drawn[0]=(*xy)[0];drawn[1]=(*xy)[1];(*g)++;
}
float getPlayer_c_screenwidth(void){return 320;}
float getPlayer_c_screenheight(void){return 240;}
float getPlayer_c_screenleft(void){return 0;}
float getPlayer_c_screentop(void){return 0;}
Gfx *native_crosshair_draw(Gfx *g,int style,float x,float y,int width,int height,int left,int top,float aspect){
 assert(style==native_setting_crosshair_shape() && width==320 && height==240 && !left && !top && aspect>0);
 draws++;custom_draws++;drawn[0]=x;drawn[1]=y;return g+1;
}
#include "native-persistent-sight.h"
#include "native-sight-draw.h"
int main(void){
 native_settings_init(NULL);Gfx commands[32];
 for(int style=0;style<8;style++){
 native_setting_set_crosshair_shape(style);
 for(int on=0;on<=1;on++)for(int aiming=0;aiming<=1;aiming++)for(int frame=0;frame<80;frame++){
  native_setting_set_always_crosshair(on);player.insightaimmode=aiming;
  player.gunsightmode=aiming?0:GUNSIGHTREASON_NOTAIMING;
  player.crosshair_angle.f[0]=30+frame*3;player.crosshair_angle.f[1]=20+frame*2;
  unsigned previous=draws;Gfx *g=commands;__typeof__(player) before=player;
  gunDrawSight((s32 *)&g);
  assert(!memcmp(&before,&player,sizeof player));
  assert(draws-previous==(unsigned)(aiming || on));
  if(draws!=previous)assert(drawn[0]==player.crosshair_angle.f[0] && drawn[1]==player.crosshair_angle.f[1]);
  if(aiming)assert(!portCrosshairPersistent());
 }
 }
 assert(custom_draws>0);
 native_setting_crosshair_shape();native_setting_set_crosshair_shape(0);player.insightaimmode=1;player.gunsightmode=0;
 for(int opacity=-1;opacity<=100;opacity=opacity<0?0:opacity+10){native_setting_set_crosshair_opacity(opacity);Gfx *g=commands;gunDrawSight((s32 *)&g);assert(sprite_alpha==native_crosshair_alpha());}

 native_setting_set_always_crosshair(1);
 for(int reason=1;reason<=16;reason*=2)if(reason!=GUNSIGHTREASON_NOTAIMING){
  player.insightaimmode=0;player.gunsightmode=reason|GUNSIGHTREASON_NOTAIMING;
  unsigned previous=draws;Gfx *g=commands;gunDrawSight((s32 *)&g);assert(draws==previous);
 }
 player.gunsightmode=0;player.mpmenuon=1;unsigned previous=draws;Gfx *g=commands;gunDrawSight((s32 *)&g);assert(draws==previous);
 puts("PASS actual sight renderer: all eight styles follow moving free-aim coordinates with persistent sight on/off, hipfire visibility only, no player-state changes and original hide reasons retained");
}
