#ifndef EOG_NATIVE_CROSSHAIR_SHAPES_H
#define EOG_NATIVE_CROSSHAIR_SHAPES_H
#include <ultra64.h>
#include "native_settings.h"
#include "native_crosshair_opacity.h"
#include "native_reticle.h"
/* Custom silhouettes are generated from geometry; Original remains the ROM
 * sprite. Coordinates are relative to the real bullet/reticle position. */
static inline int native_crosshair_pixel(int style,int x,int y){
    int ax=x<0?-x:x,ay=y<0?-y:y,r=x*x+y*y;
    if(style==1)return ax<=1 && ay<=1;
    if(style==2 || style==3 || style==6){
        int arms=(!y && ax>=3 && ax<=7) || (!x && ay>=3 && ay<=7 && (style!=6 || y>0));
        return arms || (style==3 && !x && !y);
    }
    if(style==4 || style==5)return (r>=20 && r<=36) || (style==5 && !x && !y);
    if(style==7)return ax+ay>=5 && ax+ay<=6;
    return 0;
}
static inline Gfx *native_crosshair_rect(Gfx *g,int x1,int y1,int x2,int y2,int left,int top,int right,int bottom){
    if(x1<left)x1=left;if(y1<top)y1=top;if(x2>right)x2=right;if(y2>bottom)y2=bottom;
    if(x1<x2 && y1<y2)gDPFillRectangle(g++,x1,y1,x2,y2);
    return g;
}
static inline Gfx *native_crosshair_draw(Gfx *g,int style,float cx,float cy,int width,int height,int left,int top,float aspect){
    if(style<1 || style>7 || width<=0 || height<=0)return g;
    extern void portCrosshairTint(s32 *,s32 *,s32 *);
    s32 red,green,blue;portCrosshairTint(&red,&green,&blue);
    if(!native_setting_crosshair()){red=255;green=48;blue=48;}
    unsigned opacity=native_crosshair_alpha();
    float sx=aspect>0?(4.f/3.f)/aspect:1.f;
    gDma0p(g++,0x3a,NATIVE_RETICLE_SCOPE,0xffffffu);
    gDPPipeSync(g++);gDPSetCycleType(g++,G_CYC_1CYCLE);
    gDPSetRenderMode(g++,G_RM_XLU_SURF,G_RM_XLU_SURF2);
    gDPSetCombineMode(g++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    gDPSetAlphaCompare(g++,G_AC_NONE);
    for(int pass=0;pass<2;pass++){
        gDPPipeSync(g++);
        gDPSetPrimColor(g++,0,0,pass?red:0,pass?green:0,pass?blue:0,pass?opacity:opacity*180/230);
        int outline=pass?0:1;
        for(int y=-7;y<=7;y++)for(int x=-7;x<=7;){
            if(!native_crosshair_pixel(style,x,y)){x++;continue;}
            int first=x;while(x<=7 && native_crosshair_pixel(style,x,y))x++;
            int x1=(int)(cx+(first-.5f)*sx+.5f),x2=(int)(cx+(x-.5f)*sx+.5f);
            int y1=(int)(cy+y-.5f+.5f),y2=y1+1;
            if(x2<=x1)x2=x1+1;
            g=native_crosshair_rect(g,x1-outline,y1-outline,x2+outline,y2+outline,left,top,left+width,top+height);
        }
    }
    /* Multipart silhouettes must sample stereo depth at the aiming point,
     * not at the final outline segment. Alpha zero leaves the image intact. */
    gDPPipeSync(g++);gDPSetPrimColor(g++,0,0,0,0,0,0);
    int anchor_x=(int)(cx+.5f),anchor_y=(int)(cy+.5f);
    g=native_crosshair_rect(g,anchor_x-1,anchor_y-1,anchor_x+1,anchor_y+1,left,top,left+width,top+height);
    gDPPipeSync(g++);
    gDma0p(g++,0x3a,0,(~NATIVE_RETICLE_SCOPE)&0xffffffu);
    return g;
}
#endif
