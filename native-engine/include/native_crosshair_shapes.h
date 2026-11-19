#ifndef EOG_NATIVE_CROSSHAIR_SHAPES_H
#define EOG_NATIVE_CROSSHAIR_SHAPES_H
#include <ultra64.h>
#include "native_settings.h"
#include "native_crosshair_opacity.h"
#include "native_reticle.h"
#include "native_crosshair_sdf.h"
/* Cached procedural textures from the newer Conker3DSRecomp crosshairs.
 * Original stays the ROM sprite; custom shapes use the real aiming position. */
static uint32_t native_crosshair_masks[7][64*64] __attribute__((aligned(16)));
static unsigned char native_crosshair_ready[7];
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
    gDPSetAlphaCompare(g++,G_AC_NONE);
    gDPSetTexturePersp(g++,G_TP_NONE);
    gDPSetTextureLOD(g++,G_TL_TILE);
    gDPSetTextureLUT(g++,G_TT_NONE);
    gDPSetTextureFilter(g++,G_TF_BILERP);
    gDPSetScissor(g++,G_SC_NON_INTERLACE,left,top,left+width,top+height);
    if(!native_crosshair_ready[style-1]){
        native_crosshair_sdf_generate(style,native_crosshair_masks[style-1]);
        native_crosshair_ready[style-1]=1;
    }
    /* The full tile avoids the legacy block-load cap of 2048 texels. */
    gDPLoadTextureTile(g++,native_crosshair_masks[style-1],G_IM_FMT_RGBA,G_IM_SIZ_32b,
        64,64,0,0,63,63,0,G_TX_CLAMP,G_TX_CLAMP,6,6,G_TX_NOLOD,G_TX_NOLOD);
    gDPSetPrimColor(g++,0,0,red,green,blue,opacity);
    gDPSetCombineLERP(g++,TEXEL0,0,PRIMITIVE,0,TEXEL0,0,PRIMITIVE,0,
        TEXEL0,0,PRIMITIVE,0,TEXEL0,0,PRIMITIVE,0);
    int start_x=(int)(cx-9.f*sx+.5f),start_y=(int)(cy-9.f+.5f);
    int end_x=(int)(cx+9.f*sx+.5f),end_y=start_y+18;
    int x1=start_x,y1=start_y,x2=end_x,y2=end_y;
    if(x1<left)x1=left;if(y1<top)y1=top;
    if(x2>left+width)x2=left+width;if(y2>top+height)y2=top+height;
    if(x1<x2 && y1<y2 && end_x>start_x){
        int dx=(64*1024+(end_x-start_x)/2)/(end_x-start_x);
        int dy=(64*1024+9)/18;
        int u=(x1-start_x)*64*32/(end_x-start_x),v=(y1-start_y)*64*32/18;
        gSPTextureRectangle(g++,x1*4,y1*4,x2*4,y2*4,G_TX_RENDERTILE,u,v,dx,dy);
    }
    /* Multipart silhouettes must sample stereo depth at the aiming point,
     * not at the final outline segment. Alpha zero leaves the image intact. */
    gDPPipeSync(g++);gDPSetCombineMode(g++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);gDPSetPrimColor(g++,0,0,0,0,0,0);
    int anchor_x=(int)(cx+.5f),anchor_y=(int)(cy+.5f);
    g=native_crosshair_rect(g,anchor_x-1,anchor_y-1,anchor_x+1,anchor_y+1,left,top,left+width,top+height);
    gDPPipeSync(g++);
    gDma0p(g++,0x3a,0,(~NATIVE_RETICLE_SCOPE)&0xffffffu);
    return g;
}
#endif
