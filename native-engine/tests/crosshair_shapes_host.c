#include <ultra64.h>
#include "native_crosshair_shapes.h"
#include "native_test_assert.h"
#include <string.h>
#include <stdio.h>
void portCrosshairTint(s32 *r,s32 *g,s32 *b){*r=64;*g=160;*b=255;}
int main(void){
 native_settings_init(NULL);native_setting_set_crosshair_opacity(90);
 for(int style=1;style<=7;style++)for(int aspect=0;aspect<3;aspect++)for(int edge=0;edge<5;edge++){
  Gfx commands[64];memset(commands,0,sizeof commands);
  float x=edge==1?0:edge==2?319:160,y=edge==3?0:edge==4?239:120;
  native_setting_set_crosshair(5);
  Gfx *end=native_crosshair_draw(commands,style,x,y,320,240,0,0,aspect==0?4.f/3:aspect==1?5.f/3:5.f/4);
  assert(end>commands && end<=commands+64);
  unsigned quads=0,scopes=0,blue=0,loads=0,images=0;unsigned last_colour=0;
  int last_x1=0,last_x2=0,last_y1=0,last_y2=0;
  for(Gfx *p=commands;p<end;p++){
   unsigned op=p->words.w0>>24;
   if(op==G_FILLRECT || op==G_TEXRECT){
    int x1=(p->words.w1>>12&4095)/4,y1=(p->words.w1&4095)/4;
    int x2=(p->words.w0>>12&4095)/4,y2=(p->words.w0&4095)/4;
    assert(x1>=0 && y1>=0 && x2<=320 && y2<=240 && x2>x1 && y2>y1);
    if(op==G_TEXRECT){assert(p+2<end);quads++;assert(x2-x1<=20 && y2-y1<=18);}
    else {last_x1=x1;last_x2=x2;last_y1=y1;last_y2=y2;}
   }
   if(op==G_LOADTILE){assert((p->words.w1&0xffffff)==((63u*4)<<12|63u*4));loads++;}
   if(op==G_SETTIMG){assert((p->words.w0&4095)==63 && (p->words.w0>>19&3)==G_IM_SIZ_32b);images++;}
   if(op==G_SETPRIMCOLOR){last_colour=p->words.w1;blue+=p->words.w1==0x40a0ffe6;}
   if(op==0x3a)scopes++;
  }
  assert(quads==1 && loads==1 && images==1 && blue==1 && scopes==2 && last_colour==0);
  if(!edge)assert(last_x1+last_x2==320 && last_y1+last_y2==240);
  assert(native_crosshair_ready[style-1]);
 }
 Gfx commands[64];native_setting_set_crosshair(0);
 Gfx *end=native_crosshair_draw(commands,1,160,120,320,240,0,0,4.f/3);
 int original_red=0;for(Gfx *p=commands;p<end;p++)if(p->words.w0>>24==G_SETPRIMCOLOR && p->words.w1==0xff3030e6)original_red++;
 assert(original_red==1);
 for(int opacity=-1;opacity<=100;opacity=opacity<0?0:opacity+10){
  native_setting_set_crosshair_opacity(opacity);unsigned expected=opacity<0?110u:(unsigned)(opacity*255+50)/100;
  assert(native_crosshair_alpha()==expected);
  for(int style=1;style<=7;style++){
   uint32_t before[4096];memcpy(before,native_crosshair_masks[style-1],sizeof before);
   Gfx *end=native_crosshair_draw(commands,style,160,120,320,240,0,0,4.f/3);unsigned foreground=999;
   for(Gfx *p=commands;p<end;p++)if(p->words.w0>>24==G_SETPRIMCOLOR && p->words.w1>>8==0xff3030)foreground=p->words.w1&255;
   assert(foreground==expected && !memcmp(before,native_crosshair_masks[style-1],sizeof before));
  }
 }
 assert(native_crosshair_draw(commands,0,160,120,320,240,0,0,4.f/3)==commands);
 puts("PASS smooth crosshair display lists: seven cached 64x64 textures, single quads, full tile loads, unchanged colours/opacity, all aspect ratios, clipping and centred stereo scopes");
}
