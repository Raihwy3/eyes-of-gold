#ifndef EOG_NATIVE_CROSSHAIR_SHAPES_H
#define EOG_NATIVE_CROSSHAIR_SHAPES_H
#include <ultra64.h>
#include "native_settings.h"
#include "native_crosshair_opacity.h"
#include "native_reticle.h"
/* Custom silhouettes are generated from geometry; Original remains the ROM
 * sprite. Coordinates are relative to the real bullet/reticle position. */
static inline float native_crosshair_abs(float v){return v<0.f?-v:v;}
static inline int native_crosshair_round(float v){return v>=0.f?(int)(v+.5f):(int)(v-.5f);}
static inline float native_crosshair_sqrt(float v){
    if(v<=0.f)return 0.f;
    float x=v>1.f?v*.5f:1.f;
    for(int i=0;i<6;i++)x=.5f*(x+v/x);
    return x;
}
static inline int native_crosshair_pixel(int style,int x,int y){
    int ax=x<0?-x:x,ay=y<0?-y:y,r=x*x+y*y;
    if(style==1)return r<=2;
    if(style==2 || style==3 || style==6){
        int arms=(!y && ax>=3 && ax<=7) || (!x && ay>=3 && ay<=7 && (style!=6 || y>0));
        return arms || (style==3 && !x && !y);
    }
    if(style==4 || style==5)return (r>=20 && r<=36) || (style==5 && !x && !y);
    if(style==7)return ax+ay>=5 && ax+ay<=6;
    return 0;
}
static inline Gfx *native_crosshair_rect_q(Gfx *g,int qx1,int qy1,int qx2,int qy2,int left,int top,int right,int bottom){
    int ql=left*4,qt=top*4,qr=right*4,qb=bottom*4;
    if(qx1<ql)qx1=ql;if(qy1<qt)qy1=qt;if(qx2>qr)qx2=qr;if(qy2>qb)qy2=qb;
    if((qx2>>2)>(qx1>>2) && (qy2>>2)>(qy1>>2)){
        g->words.w0=((unsigned)G_FILLRECT<<24)|(((unsigned)qx2&4095u)<<12)|((unsigned)qy2&4095u);
        g->words.w1=(((unsigned)qx1&4095u)<<12)|((unsigned)qy1&4095u);
        g++;
    }
    return g;
}
static inline Gfx *native_crosshair_rect(Gfx *g,int x1,int y1,int x2,int y2,int left,int top,int right,int bottom){
    return native_crosshair_rect_q(g,x1*4,y1*4,x2*4,y2*4,left,top,right,bottom);
}
static inline Gfx *native_crosshair_span(Gfx *g,float cx,float cy,float sx,float y,float x_lo,float x_hi,int left,int top,int right,int bottom){
    int qx1=native_crosshair_round((cx+x_lo*sx)*4.f);
    int qx2=native_crosshair_round((cx+x_hi*sx)*4.f);
    int qy1=native_crosshair_round((cy+y)*4.f);
    if((qx2>>2)<=(qx1>>2))qx2=((qx1>>2)+1)*4;
    return native_crosshair_rect_q(g,qx1,qy1,qx2,qy1+4,left,top,right,bottom);
}
static inline Gfx *native_crosshair_ring_row(Gfx *g,float cx,float cy,float sx,float y,float r_in,float r_out,int left,int top,int right,int bottom){
    float ay=native_crosshair_abs(y);
    if(ay>=r_out)return g;
    float outer=native_crosshair_sqrt(r_out*r_out-ay*ay);
    if(r_in<=0.f || ay>=r_in){
        return native_crosshair_span(g,cx,cy,sx,y,-outer,outer,left,top,right,bottom);
    }
    float inner=native_crosshair_sqrt(r_in*r_in-ay*ay);
    g=native_crosshair_span(g,cx,cy,sx,y,-outer,-inner,left,top,right,bottom);
    return native_crosshair_span(g,cx,cy,sx,y,inner,outer,left,top,right,bottom);
}
static inline Gfx *native_crosshair_draw(Gfx *g,int style,float cx,float cy,int width,int height,int left,int top,float aspect){
    if(style<1 || style>7 || width<=0 || height<=0)return g;
    extern void portCrosshairTint(s32 *,s32 *,s32 *);
    s32 red,green,blue;portCrosshairTint(&red,&green,&blue);
    if(!native_setting_crosshair()){red=255;green=48;blue=48;}
    unsigned opacity=native_crosshair_alpha();
    float sx=aspect>0?(4.f/3.f)/aspect:1.f;
    int right=left+width,bottom=top+height;
    gDma0p(g++,0x3a,NATIVE_RETICLE_SCOPE,0xffffffu);
    gDPPipeSync(g++);gDPSetCycleType(g++,G_CYC_1CYCLE);
    gDPSetRenderMode(g++,G_RM_XLU_SURF,G_RM_XLU_SURF2);
    gDPSetCombineMode(g++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    gDPSetAlphaCompare(g++,G_AC_NONE);
    for(int pass=0;pass<2;pass++){
        gDPPipeSync(g++);
        gDPSetPrimColor(g++,0,0,pass?red:0,pass?green:0,pass?blue:0,pass?opacity:opacity*180/230);
        float pad=pass?0.f:0.75f;
        if(style==1){
            float r=1.55f+pad;
            for(int iy=-2;iy<=2;iy++)g=native_crosshair_ring_row(g,cx,cy,sx,(float)iy-.5f,0.f,r,left,top,right,bottom);
        }else if(style==2 || style==3 || style==6){
            float half=0.55f+pad;
            /* Left and right horizontal arms */
            int qy1=native_crosshair_round((cy-half)*4.f),qy2=native_crosshair_round((cy+half)*4.f);
            if((qy2>>2)<=(qy1>>2))qy2=((qy1>>2)+1)*4;
            int ql1=native_crosshair_round((cx+(-7.2f-pad)*sx)*4.f),ql2=native_crosshair_round((cx+(-2.6f+pad)*sx)*4.f);
            int qr1=native_crosshair_round((cx+(2.6f-pad)*sx)*4.f),qr2=native_crosshair_round((cx+(7.2f+pad)*sx)*4.f);
            if((ql2>>2)<=(ql1>>2))ql2=((ql1>>2)+1)*4;
            if((qr2>>2)<=(qr1>>2))qr2=((qr1>>2)+1)*4;
            g=native_crosshair_rect_q(g,ql1,qy1,ql2,qy2,left,top,right,bottom);
            g=native_crosshair_rect_q(g,qr1,qy1,qr2,qy2,left,top,right,bottom);
            /* Vertical arms */
            int qx1=native_crosshair_round((cx-half*sx)*4.f),qx2=native_crosshair_round((cx+half*sx)*4.f);
            if((qx2>>2)<=(qx1>>2))qx2=((qx1>>2)+1)*4;
            if(style!=6){
                int qt1=native_crosshair_round((cy-7.2f-pad)*4.f),qt2=native_crosshair_round((cy-2.6f+pad)*4.f);
                if((qt2>>2)<=(qt1>>2))qt2=((qt1>>2)+1)*4;
                g=native_crosshair_rect_q(g,qx1,qt1,qx2,qt2,left,top,right,bottom);
            }
            int qb1=native_crosshair_round((cy+2.6f-pad)*4.f),qb2=native_crosshair_round((cy+7.2f+pad)*4.f);
            if((qb2>>2)<=(qb1>>2))qb2=((qb1>>2)+1)*4;
            g=native_crosshair_rect_q(g,qx1,qb1,qx2,qb2,left,top,right,bottom);
            if(style==3){
                float dr=1.05f+pad;
                for(int iy=-1;iy<=1;iy++)g=native_crosshair_ring_row(g,cx,cy,sx,(float)iy-.5f,0.f,dr,left,top,right,bottom);
            }
        }else if(style==4 || style==5){
            float r_in=4.35f-pad,r_out=5.65f+pad;
            if(r_in<0.f)r_in=0.f;
            for(int iy=-6;iy<=6;iy++){
                float y=(float)iy-.5f;
                g=native_crosshair_ring_row(g,cx,cy,sx,y,r_in,r_out,left,top,right,bottom);
            }
            if(style==5){
                float dr=1.05f+pad;
                for(int iy=-1;iy<=1;iy++)g=native_crosshair_ring_row(g,cx,cy,sx,(float)iy-.5f,0.f,dr,left,top,right,bottom);
            }
        }else if(style==7){
            float d_in=4.35f-pad,d_out=5.65f+pad;
            for(int iy=-6;iy<=6;iy++){
                float y=(float)iy-.5f,ay=native_crosshair_abs(y);
                if(ay>=d_out)continue;
                float outer=d_out-ay;
                if(ay>=d_in){
                    g=native_crosshair_span(g,cx,cy,sx,y,-outer,outer,left,top,right,bottom);
                }else{
                    float inner=d_in-ay;
                    g=native_crosshair_span(g,cx,cy,sx,y,-outer,-inner,left,top,right,bottom);
                    g=native_crosshair_span(g,cx,cy,sx,y,inner,outer,left,top,right,bottom);
                }
            }
        }
    }
    /* Multipart silhouettes must sample stereo depth at the aiming point,
     * not at the final outline segment. Alpha zero leaves the image intact. */
    gDPPipeSync(g++);gDPSetPrimColor(g++,0,0,0,0,0,0);
    int anchor_x=(int)(cx+.5f),anchor_y=(int)(cy+.5f);
    g=native_crosshair_rect(g,anchor_x-1,anchor_y-1,anchor_x+1,anchor_y+1,left,top,right,bottom);
    gDPPipeSync(g++);
    gDma0p(g++,0x3a,0,(~NATIVE_RETICLE_SCOPE)&0xffffffu);
    return g;
}
#endif
