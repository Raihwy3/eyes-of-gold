#include <ultra64.h>
#include "native_crosshair_shapes.h"
#include "native_test_assert.h"
#include <string.h>
#include <stdio.h>
void portCrosshairTint(s32 *r,s32 *g,s32 *b){*r=64;*g=160;*b=255;}
int main(void){
    native_settings_init(NULL);native_setting_set_crosshair_opacity(90);
    for(int style=1;style<=7;style++)for(int aspect=0;aspect<3;aspect++)for(int edge=0;edge<5;edge++){
        Gfx commands[256];memset(commands,0,sizeof commands);
        float x=edge==1?0:edge==2?319:160,y=edge==3?0:edge==4?239:120;
        native_setting_set_crosshair(5);
        Gfx *end=native_crosshair_draw(commands,style,x,y,320,240,0,0,aspect==0?4.f/3:aspect==1?5.f/3:5.f/4);
        assert(end>commands && end<=commands+256);
        unsigned rects=0,scopes=0,blue=0,black=0;
        int last_x1=0,last_x2=0,last_y1=0,last_y2=0;unsigned last_colour=0;
        for(Gfx *p=commands;p<end;p++){
            unsigned op=p->words.w0>>24;
            if(op==G_FILLRECT){
                int x1=(p->words.w1>>12&4095)/4,y1=(p->words.w1&4095)/4;
                int x2=(p->words.w0>>12&4095)/4,y2=(p->words.w0&4095)/4;
                last_x1=x1;last_x2=x2;last_y1=y1;last_y2=y2;
                assert(x1>=0 && y1>=0 && x2<=320 && y2<=240 && x2>x1 && y2>y1);rects++;
            }
            if(op==G_SETPRIMCOLOR){last_colour=p->words.w1;blue+=p->words.w1==0x40a0ffe6;black+=p->words.w1==0x000000b4;}
            if(op==0x3a)scopes++;
        }
        assert(rects && blue==1 && black==1 && scopes==2 && last_colour==0);
        if(!edge)assert(last_x1+last_x2==320 && last_y1+last_y2==240);
    }
    for(int style=1;style<=7;style++){
        int pixels=0;
        for(int y=-7;y<=7;y++)for(int x=-7;x<=7;x++)pixels+=native_crosshair_pixel(style,x,y)!=0;
        assert(pixels>0 && pixels<100);
    }
    assert(!native_crosshair_pixel(2,0,0) && native_crosshair_pixel(3,0,0));
    assert(!native_crosshair_pixel(4,0,0) && native_crosshair_pixel(5,0,0));
    assert(!native_crosshair_pixel(6,0,-5) && native_crosshair_pixel(6,0,5));
    Gfx commands[256];native_setting_set_crosshair(0);
    Gfx *end=native_crosshair_draw(commands,1,160,120,320,240,0,0,4.f/3);
    int original_red=0;for(Gfx *p=commands;p<end;p++)if(p->words.w0>>24==G_SETPRIMCOLOR && p->words.w1==0xff3030e6)original_red++;
    assert(original_red==1);
    for(int opacity=-1;opacity<=100;opacity=opacity<0?0:opacity+10){
        native_setting_set_crosshair_opacity(opacity);
        unsigned expected=opacity<0?110u:(unsigned)(opacity*255+50)/100;
        assert(native_crosshair_alpha()==expected);
        for(int shape=1;shape<=7;shape++){
            Gfx *end=native_crosshair_draw(commands,shape,160,120,320,240,0,0,4.f/3);
            unsigned foreground=0,outline=0;
            for(Gfx *p=commands;p<end;p++)if(p->words.w0>>24==G_SETPRIMCOLOR){
                if((p->words.w1>>8)==0xff3030)foreground=p->words.w1&255;
                if((p->words.w1>>8)==0 && (p->words.w1&255)!=0)outline=p->words.w1&255;
            }
            assert(foreground==expected && outline==expected*180/230);
        }
    }
    puts("PASS custom crosshair display lists: seven silhouettes, existing colours, black outline, all aspect ratios, edge clipping and balanced stereo scope");
}
