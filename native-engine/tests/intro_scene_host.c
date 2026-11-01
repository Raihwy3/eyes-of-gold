#include "native_effects.h"
#include "native_scenery_scope.h"
#include <ultra64.h>
#include "gbi_extension.h"
#include "native_graphics.h"
#include "native_viewmodel.h"
#include "title.h"
static Mtx matrices[7][2];
static Gfx commands[4096],hole[64];
static Vtx vertices[32];
static Vp viewport={{{880,660,511,0},{880,660,511,0}}};
extern Gfx *load_display_rare_logo(Gfx *,s32,s32,s32,s32);
extern Gfx *manipulateGunbarrelAndLogoMatrices(Gfx *);
extern Gfx *sub_GAME_7F01BFF8(Gfx *,Vtx *,s32);
extern void createGunbarrelRenderHole(void *,s32);
extern Gfx *gunbarrelgfxListPointer;
extern f32 g_TitleX,g_TitleY,titleTransitionX,titleTransitionY;
/* Only original clear_framebuffer_black's platform VI boundary is supplied. */
s16 viGetX(void) {return 440;}
s16 viGetY(void) {return 330;}
void *viGetFrameBuf2(void) {static unsigned short frame[440*330];return frame;}
void native_intro_scene(int phase) {
    matrixBufferRareLogo0=matrices[0];matrixBufferGunbarrel0=matrices[1];
    matrixBufferRareLogo1=matrices[2];matrixBufferRareLogo2=matrices[3];matrixBufferGunbarrel1=matrices[4];
    guOrtho(matrixBufferGunbarrel0,0,1280,0,960,1,8,256);
    createGunbarrelRenderHole(vertices,30);sub_GAME_7F01BFF8(hole,vertices,30);
    gunbarrelgfxListPointer=hole;D_8002A7D0=0;
    Gfx *p=commands;
    gSPViewport(p++,&viewport);
    gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,440,330);
    if(phase==0)p=load_display_rare_logo(p,403,488,880,255);
    else {
        g_TitleX=640;g_TitleY=482;titleTransitionX=400;titleTransitionY=482;
        p=manipulateGunbarrelAndLogoMatrices(p);
    }
    gSPEndDisplayList(p++);native_graphics_render(commands);
}

/* A foreground triangle crosses the camera plane with one corner at W=0.
 * Backface rejection must happen before near clipping without any division
 * by that corner. Retained triangles become two after proper near clipping. */
static void native_camera_plane_scene_impl(int reverse,int front_cull,int scoped) {
    static Mtx proj,model;
    float matrix[4][4]={{.5f,0,0,0},{0,.5f,0,0},{0,0,.5f,1},{0,0,0,1}};
    guMtxF2L(matrix,&proj);guScale(&model,1,1,1);
    memset(vertices,0,sizeof vertices);
    vertices[0].v.ob[0]=-1;vertices[0].v.ob[1]=-1;vertices[0].v.ob[2]=1;
    vertices[1].v.ob[0]=1;vertices[1].v.ob[1]=-1;vertices[1].v.ob[2]=1;
    vertices[2].v.ob[0]=0;vertices[2].v.ob[1]=1;vertices[2].v.ob[2]=-1;
    for(int i=0;i<3;++i)memset(vertices[i].v.cn,255,4);
    Gfx *p=commands;
    if(scoped){native_viewmodel_scope(p++,1);}
    gSPViewport(p++,&viewport);
    gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,viewport.vp.vscale[0]/2,viewport.vp.vscale[1]/2);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_FOG|G_LIGHTING|G_ZBUFFER);
    gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|(front_cull?G_CULL_FRONT:G_CULL_BACK));
    gDPSetCycleType(p++,G_CYC_1CYCLE);
    gDPSetRenderMode(p++,G_RM_OPA_SURF,G_RM_OPA_SURF2);
    gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gSPMatrix(p++,&proj,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&model,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPVertex(p++,vertices,3,0);
    if(reverse) {gSP1Triangle(p++,2,1,0,0);}
    else {gSP1Triangle(p++,0,1,2,0);}
    if(scoped){native_viewmodel_scope(p++,0);}
    gSPEndDisplayList(p++);native_graphics_render(commands);
}

void native_camera_plane_scene(int reverse,int front_cull){native_camera_plane_scene_impl(reverse,front_cull,1);}
void native_world_camera_plane_scene(int reverse,int front_cull){native_camera_plane_scene_impl(reverse,front_cull,0);}

#include "floatvtx.h"
void native_world_winding_scene(int reverse,int front_cull) {
    static PortFloatVtx v[3];static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    memset(v,0,sizeof v);
    v[0].x=10000;v[0].y=10000;v[1].x=10001;v[1].y=10002;
    v[2].x=10002;v[2].y=10004+9/1024.f;
    for(int i=0;i<3;i++){v[i].w=65535;v[i].r=v[i].g=v[i].b=v[i].a=255;}
    Gfx *p=commands;gSPViewport(p++,&vp);gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPClearGeometryMode(p++,0xffffffff);
    gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER|(front_cull?G_CULL_FRONT:G_CULL_BACK));
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    native_scenery_scope(p++,1);gSPFloatVertexExt(p++,v,3,0);
    if(reverse){gSP1Triangle(p++,2,1,0,0);}else {gSP1Triangle(p++,0,1,2,0);}
    native_scenery_scope(p++,0);gSPEndDisplayList(p++);native_graphics_render(commands);
}

void native_lcd_edge_scene(void) {
    Vp saved=viewport;
    viewport.vp.vscale[0]=viewport.vp.vtrans[0]=640;
    viewport.vp.vscale[1]=viewport.vp.vtrans[1]=480;
    native_camera_plane_scene(0,0);
    viewport=saved;
}

#include "native_theme.h"
void native_watch_colour_scene(void) {
    static Mtx proj,model;
    guOrtho(&proj,-10,10,-10,10,-10,10,1);guScale(&model,1,1,1);
    memset(vertices,0,sizeof vertices);
    vertices[0].v.ob[0]=-5;vertices[0].v.ob[1]=-5;
    vertices[1].v.ob[0]=5;vertices[1].v.ob[1]=-5;
    vertices[2].v.ob[1]=5;
    for(int i=0;i<3;i++){vertices[i].v.cn[1]=255;vertices[i].v.cn[3]=123;}
    static Vp lcd={{{640,480,511,0},{640,480,511,0}}};
    Gfx *p=commands;
    gSPViewport(p++,&lcd);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_FOG|G_LIGHTING|G_ZBUFFER);
    gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH);
    gDPSetCycleType(p++,G_CYC_1CYCLE);
    gDPSetRenderMode(p++,G_RM_XLU_SURF,G_RM_XLU_SURF2);
    gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gSPMatrix(p++,&proj,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&model,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPVertex(p++,vertices,3,0);
    gSP1Triangle(p++,0,1,2,0);
    gDma0p(p++,0x3a,NATIVE_WATCH_SCOPE,0xffffffu);
    gSP1Triangle(p++,0,1,2,0);
    gDma0p(p++,0x3a,0,(~NATIVE_WATCH_SCOPE)&0xffffffu);
    gSP1Triangle(p++,0,1,2,0);
    gSPEndDisplayList(p++);native_graphics_render(commands);
}

#include "native_glass.h"
static void native_glass_scene_impl(int opacity,int batch,int tier,int simple) {
    static Mtx proj,model;static u8 white[64];memset(white,255,sizeof white);
    guOrtho(&proj,-10,10,-10,10,-10,10,1);guScale(&model,1,1,1);
    memset(vertices,0,sizeof vertices);
    vertices[0].v.ob[0]=-5;vertices[0].v.ob[1]=-5;
    vertices[1].v.ob[0]=5;vertices[1].v.ob[1]=-5;vertices[2].v.ob[1]=5;
    for(int i=0;i<3;i++){vertices[i].v.cn[2]=127;vertices[i].v.cn[3]=255;}
    static Vp lcd={{{640,480,511,0},{640,480,511,0}}};
    Gfx *p=commands;gSPViewport(p++,&lcd);
    gSPClearGeometryMode(p++,G_CULL_BOTH|G_FOG);
    gSPSetGeometryMode(p++,G_LIGHTING|G_TEXTURE_GEN|G_ZBUFFER);
    gSPTexture(p++,65535,65535,0,G_TX_RENDERTILE,G_ON);
    gDPLoadTextureBlock(p++,white,G_IM_FMT_I,G_IM_SIZ_8b,8,8,0,G_TX_WRAP,G_TX_WRAP,3,3,0,0);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetRenderMode(p++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);
    gDPSetCombineMode(p++,G_CC_MODULATEIA_PRIM,G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(p++,0,0,100,120,140,opacity);
    gSPMatrix(p++,&proj,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&model,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPVertex(p++,vertices,3,0);gSP1Triangle(p++,0,1,2,0);
    native_glass_scope(p++,1);
    if(simple>=0)native_glass_detail_command(p++,simple,tier);
    if(batch)native_simple_glass_scope(p++,1);
    gSPVertex(p++,vertices,3,0);gSP1Triangle(p++,0,1,2,0);
    if(batch)native_simple_glass_scope(p++,0);
    native_glass_scope(p++,0);
    gSPVertex(p++,vertices,3,0);gSP1Triangle(p++,0,1,2,0);
    gSPEndDisplayList(p++);native_graphics_render(commands);
}
void native_glass_scene(int opacity){native_glass_scene_impl(opacity,0,0,-1);}
void native_glass_gpu_scene(int opacity,int tier,int simple){native_glass_scene_impl(opacity,simple,tier,simple);}

/* Retail fog with no custom fog scope, crossing the camera/near plane. */
static void native_original_fog_near_impl(int offset,int x) {
    static Vtx v[6];static Mtx projection,model;static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    u16 norm;guPerspective(&projection,&norm,60,4.f/3.f,5,1000,1);guTranslate(&model,x,0,offset);
    for(int i=0;i<6;i++) {
        memset(&v[i],0,sizeof v[i]);v[i].v.ob[0]=i%3==0?-1:i%3==1?1:0;v[i].v.ob[1]=i%3==2?1:-1;
        v[i].v.ob[2]=i==0?-2:i<3?-30:-60;
        v[i].v.cn[0]=30+i*20;v[i].v.cn[1]=160;v[i].v.cn[2]=80;v[i].v.cn[3]=255;
    }
    Gfx *p=commands;gSPViewport(p++,&vp);gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPClearGeometryMode(p++,0xffffffff);gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER|G_FOG);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);gDPSetFogColor(p++,20,80,180,255);gSPFogPosition(p++,970,1000);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);gSPMatrix(p++,&model,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    native_scenery_scope(p++,1);gSPVertex(p++,v,6,0);gSP1Triangle(p++,0,1,2,0);gSP1Triangle(p++,3,4,5,0);
    native_scenery_scope(p++,0);gSPEndDisplayList(p++);native_graphics_render(commands);
}
void native_original_fog_near_scene(int offset){native_original_fog_near_impl(offset,0);}
void native_gpu_side_scene(int x){native_original_fog_near_impl(-100,x);}

/* Deferred vertices must retain load-time state through partial overwrites,
 * matrix/fog/texture-scale changes, modified UVs and unscoped CPU draws. */
void native_deferred_bg_scene(void) {
    static Vtx v[3];static Mtx projection,model[2];static u8 white[128];
    static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    u16 norm;guPerspective(&projection,&norm,60,4.f/3.f,5,1000,1);
    guTranslate(&model[0],1000,0,0);guMtxIdent(&model[1]);memset(white,255,sizeof white);
    for(int i=0;i<3;i++) {
        memset(&v[i],0,sizeof v[i]);v[i].v.ob[0]=i==0?-10:i==1?10:0;
        v[i].v.ob[1]=i==2?10:-10;v[i].v.ob[2]=-800;
        v[i].v.tc[0]=i*64;v[i].v.tc[1]=128-i*32;
        v[i].v.cn[0]=80+i*20;v[i].v.cn[1]=150;v[i].v.cn[2]=100;v[i].v.cn[3]=255;
    }
    Gfx *p=commands;gSPViewport(p++,&vp);gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPClearGeometryMode(p++,0xffffffff);gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER|G_FOG);
    gSPTexture(p++,65535,65535,0,G_TX_RENDERTILE,G_ON);
    gDPLoadTextureBlock(p++,white,G_IM_FMT_RGBA,G_IM_SIZ_16b,8,8,0,G_TX_WRAP,G_TX_WRAP,3,3,0,0);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_MODULATEIA,G_CC_MODULATEIA);
    gDPSetRenderMode(p++,G_RM_FOG_SHADE_A,G_RM_AA_ZB_OPA_SURF2);
    gDPSetFogColor(p++,20,80,180,255);gSPFogPosition(p++,970,1000);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&model[0],G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    native_scenery_scope(p++,1);gSPVertex(p++,v,3,0);gSP1Triangle(p++,0,1,2,0);
    gSPMatrix(p++,&model[1],G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPFogPosition(p++,0,1000);gSPTexture(p++,32768,16384,0,G_TX_RENDERTILE,G_ON);
    gSPVertex(p++,v,1,0);gSP1Triangle(p++,0,1,2,0);
    gSPVertex(p++,v+1,2,1);gSPModifyVertex(p++,1,G_MWO_POINT_ST,(32u<<16)|64u);
    gSP1Triangle(p++,0,1,2,0);
    native_scenery_scope(p++,0);gSP1Triangle(p++,0,1,2,0);
    gSPEndDisplayList(p++);native_graphics_render(commands);
}

#include "native_distance_fog.h"
/* Exercise real interpreter clipping, including a wall spanning both planes. */
void native_distance_fog_scene(int mode) {
    static struct NativeDistanceFog fog;
    static Vtx v[18];static Mtx projection,identity;
    static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    u16 norm;guPerspective(&projection,&norm,60,4.f/3.f,5,1000,1);guMtxIdent(&identity);
    native_distance_fog_plan(5,1000,200,0x103060ff,&fog);
    int depths[6][3]={{50,50,50},{160,160,160},{400,400,400},{100,100,400},{-5,50,50},{-5,100,400}};
    for(int t=0;t<6;t++)for(int i=0;i<3;i++){
        Vtx *p=&v[t*3+i];memset(p,0,sizeof *p);
        p->v.ob[0]=i==0?-10:i==1?10:0;p->v.ob[1]=i==2?10:-10;
        p->v.ob[2]=-depths[t][i];memset(p->v.cn,255,4);
    }
    Gfx *p=commands;gSPViewport(p++,&vp);
    gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&identity,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPClearGeometryMode(p++,0xffffffff);gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    native_scenery_scope(p++,1);
    native_distance_fog_command(p++,(mode&1)?&fog:NULL);
    if(mode>=2){
        gSPVertex(p++,v,3,0);gSP1Triangle(p++,0,1,2,0);
        gSPVertex(p++,v+6,3,0);for(int i=0;i<100;i++)gSP1Triangle(p++,0,1,2,0);
    }else for(int t=0;t<6;t++){gSPVertex(p++,v+t*3,3,0);gSP1Triangle(p++,0,1,2,0);}
    native_distance_fog_command(p++,(struct NativeDistanceFog *)NULL);
    native_scenery_scope(p++,0);
    // A following non-fog draw proves the scope does not leak into HUD/UI.
    gSPVertex(p++,v+6,3,0);gSP1Triangle(p++,0,1,2,0);
    gSPEndDisplayList(p++);native_graphics_render(commands);
}

/* Long consecutive runs exceed the reference VBO boundary and reuse command
 * storage after mutation. Real GBI macros supply both triangle encodings. */
void native_scenery_batch_scene(int mode) {
    int faded=mode>=12,actor=(mode>=6 && mode<9) || faded,vehicle=mode>=9 && !faded,object=mode>=3;mode%=3;
    static Vtx v[6];static Mtx projection,identity,bone;
    static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    guOrtho(&projection,-20,20,-20,20,1,100,1);guMtxIdent(&identity);
    memset(v,0,sizeof v);
    for(int i=0;i<6;i++) {
        v[i].v.ob[0]=(i%3==0?-10:i%3==1?10:0)+(mode==2?2:0);
        v[i].v.ob[1]=i%3==2?10:-10;v[i].v.ob[2]=i<3?-10:-20;
        v[i].v.cn[0]=i<3?255:40;v[i].v.cn[1]=i<3?30:255;v[i].v.cn[2]=80;v[i].v.cn[3]=255;
    }
    Gfx *p=commands;gSPViewport(p++,&vp);
    gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&identity,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPClearGeometryMode(p++,0xffffffff);gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    if(faded){gDPSetRenderMode(p++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);}
    if(object) {
        static Lights1 lights=gdSPDefLights1(60,60,60,180,150,110,0,0,127);
        lights.a.l.col[0]=lights.a.l.colc[0]=mode==1?120:60;
        gSPSetGeometryMode(p++,G_LIGHTING);gSPSetLights1(p++,lights);
        gDPSetEnvColor(p++,mode==1?80:160,100,200,255);
        guTranslate(&identity,mode==2?3.f:0.f,0,0);
        gSPMatrix(p++,&identity,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        if(faded){native_faded_character_scope(p++,1);}else if(actor){native_character_scope(p++,1);}else if(vehicle){native_vehicle_scope(p++,1);}else {native_object_scope(p++,1);}
    }else {native_scenery_scope(p++,1);}
    gSPVertex(p++,v,6,0);
    for(int i=0;i<330;i++) {
        if(mode==1 && i%3==0){gSP4Triangles(p++,0,1,2,3,4,5,0,0,0,0,0,0);}
        else if(mode==2 && i%2){gSP1Triangle(p++,3,4,5,0);}
        else {gSP1Triangle(p++,0,1,2,0);}
    }
    if(actor) {
        guTranslate(&bone,mode==2?-3.f:2.f,1.f,0);
        gSPMatrix(p++,&bone,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        gSPVertex(p++,v,6,0);gSP1Triangle(p++,3,4,5,0);
        native_character_scope(p++,0);
    }else if(vehicle){native_vehicle_scope(p++,0);}else if(object){native_object_scope(p++,0);}else {native_scenery_scope(p++,0);}
    gSPEndDisplayList(p++);native_graphics_render(commands);
}

static int native_face_colour_variant;
void native_model_face_scene(int kind,int front,int back_only,int pose) {
    static Vtx v[6];static Mtx projection,model;static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    u16 norm;guPerspective(&projection,&norm,60,4.f/3.f,5,1000,1);guRotate(&model,pose,0,1,0);
    for(int i=0;i<6;i++) {
        memset(&v[i],0,sizeof v[i]);v[i].v.ob[0]=i%3==0?-10:i%3==1?10:0;
        v[i].v.ob[1]=i%3==2?10:-10;v[i].v.ob[2]=-100-(i>=3?20:0);
        v[i].v.cn[0]=80+i*20+native_face_colour_variant;v[i].v.cn[1]=160;v[i].v.cn[2]=100;v[i].v.cn[3]=255;
    }
    Gfx *p=commands;gSPViewport(p++,&vp);gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPClearGeometryMode(p++,0xffffffff);
    gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER|(front?G_CULL_FRONT:G_CULL_BACK));
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPMatrix(p++,&model,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gDma0p(p++,NATIVE_SCENERY_COMMAND,0,kind);gSPVertex(p++,v,6,0);
    for(int i=0;i<16;i++) {
        if(back_only){gSP1Triangle(p++,0,2,1,0);}else {gSP1Triangle(p++,0,1,2,0);}
        gSP1Triangle(p++,3,5,4,0);
    }
    native_scenery_scope(p++,0);gSPEndDisplayList(p++);native_graphics_render(commands);
}

void native_cpu_cache_scene(int variant) {
    native_face_colour_variant=variant;
    native_model_face_scene(1,0,0,0);
    native_face_colour_variant=0;
}

void native_cpu_cache_pose_scene(int variant,int pose,int reference) {
    native_face_colour_variant=variant;
    native_model_face_scene(reference?0:1,0,0,pose);
    native_face_colour_variant=0;
}

void native_cpu_room_budget_scene(int reference) {
    static Vtx v[3];static Mtx projection,model[270];static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    u16 norm;guPerspective(&projection,&norm,60,4.f/3.f,5,1000,1);
    for(int i=0;i<3;i++){memset(v+i,0,sizeof v[i]);v[i].v.ob[0]=i==0?-10:i==1?10:0;v[i].v.ob[1]=i==2?10:-10;v[i].v.ob[2]=-100;memset(v[i].v.cn,255,4);}
    Gfx *p=commands;gSPViewport(p++,&vp);gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPClearGeometryMode(p++,0xffffffff);gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);
    native_scenery_scope(p++,!reference);
    for(int i=0;i<270;i++) {
        guTranslate(model+i,(i-135)*.2f,0,0);
        gSPMatrix(p++,model+i,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        gSPVertex(p++,v,3,0);gSP1Triangle(p++,0,1,2,0);
    }
    native_scenery_scope(p++,0);gSPEndDisplayList(p++);native_graphics_render(commands);
}

#include "native_lod_scope.h"
#include "native_baked_lod.h"
#include "native_test_assert.h"
void native_generated_lod_scene(int depth,int object) {
    static Vtx v[25];static Mtx projection,model;
    static Vp vp={{{640,480,511,0},{640,480,511,0}}};
    static struct NativeLodScope scope={1.f};
    static short target[25*3];static struct NativeBakedLodPacket baked;
    u16 norm;guPerspective(&projection,&norm,60,4.f/3.f,5,5000,1);guTranslate(&model,-200,-200,-depth);
    memset(v,0,sizeof v);
    for(int y=0;y<5;y++)for(int x=0;x<5;x++) {
        Vtx *p=&v[y*5+x];p->v.ob[0]=x*100;p->v.ob[1]=y*100;
        p->v.tc[0]=x*64;p->v.tc[1]=y*64;memset(p->v.cn,127,4);
    }
    Gfx *p=commands;gSPViewport(p++,&vp);gDPSetScissor(p++,G_SC_NON_INTERLACE,0,0,320,240);
    gSPMatrix(p++,&projection,G_MTX_PROJECTION|G_MTX_LOAD|G_MTX_NOPUSH);gSPMatrix(p++,&model,G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
    gSPClearGeometryMode(p++,0xffffffff);gSPSetGeometryMode(p++,G_SHADE|G_SHADING_SMOOTH|G_ZBUFFER);
    gDPSetCycleType(p++,G_CYC_1CYCLE);gDPSetCombineMode(p++,G_CC_SHADE,G_CC_SHADE);
    gDPSetRenderMode(p++,G_RM_AA_ZB_OPA_SURF,G_RM_AA_ZB_OPA_SURF2);gDPSetTextureLOD(p++,G_TL_TILE);
    native_lod_scope(p++,&scope);
    if(object>=8){native_effect_scope(p++,object-8,1);}else if(object==7){native_faded_character_scope(p++,1);gDPSetRenderMode(p++,G_RM_AA_ZB_XLU_SURF,G_RM_AA_ZB_XLU_SURF2);}else if(object && object!=6){native_object_scope(p++,1);}else{native_scenery_scope(p++,1);}
    if(object==2 || object==7) {
        for(int i=0;i<25;i++)for(int k=0;k<3;k++)target[i*3+k]=v[i].v.ob[k];
        for(int k=0;k<3;k++)target[12*3+k]=v[7].v.ob[k];
        baked=(struct NativeBakedLodPacket){v,target,25};native_baked_lod_command(p++,&baked);
    }
    if(object==6) {
        gDPSetEnvColor(p++,100,150,200,255);
        gDPSetCombineLERP(p++,SHADE,0,ENVIRONMENT,0,0,0,0,SHADE,SHADE,0,ENVIRONMENT,0,0,0,0,SHADE);
    }
    if(object==5) {
        gDPSetTextureLOD(p++,G_TL_LOD);
        gDPSetCombineLERP(p++,SHADE,0,LOD_FRACTION,0,0,0,0,SHADE,SHADE,0,LOD_FRACTION,0,0,0,0,SHADE);
    }
    if(object==3){gSPSetGeometryMode(p++,G_CULL_FRONT);}
    if(object==4){gSPSetGeometryMode(p++,G_CULL_BACK);}
    gSPVertex(p++,v,25,0);
    for(int y=0;y<4;y++)for(int x=0;x<4;x++) {
        int a=y*5+x;gSP1Triangle(p++,a,a+1,a+5,0);gSP1Triangle(p++,a+1,a+6,a+5,0);
    }
    if(object>=8){native_effect_scope(p++,0,0);}else if(object && object!=6){native_object_scope(p++,0);}else{native_scenery_scope(p++,0);}
    if(object==2 || object==7)native_baked_lod_command(p++,(struct NativeBakedLodPacket *)NULL);
    native_lod_scope(p++,(struct NativeLodScope *)NULL);
    gSPEndDisplayList(p++);native_graphics_render(commands);
    assert(v[12].v.ob[0]==200 && v[12].v.ob[1]==200);
}
