/* Included after bondview2.c so original matrix/arena functions retain ABI. */
#include "native_watch_skin_mesh.h"
#include "native_settings.h"
static ModelFileHeader *native_skin_header;
static int native_skin_current;
int native_watch_skin_active;
int native_watch_custom_part(const ModelNode *node) {
    if(!native_skin_header || !native_skin_current)return 0;
    for(unsigned depth=0;node && depth<64;depth++,node=node->Parent)
        for(int i=0;i<3;i++)if(node==native_skin_header->Switches[i])return 1;
    return 0;
}
static void native_watch_skin_begin(ModelFileHeader *header){
    native_skin_current=native_setting_skin();
    native_skin_header=native_skin_current && header && header->Switches?header:NULL;
    native_watch_skin_active=native_skin_header!=NULL;
}
static void native_watch_skin_end(void){native_skin_header=NULL;native_watch_skin_active=0;}
static Gfx *native_watch_skin_polygon(Gfx *gdl,const struct NativeWatchHand *p,int y){
    if(p->count<3)return gdl;
    Vtx *v=dynAllocateVertices(p->count);
    for(unsigned i=0;i<p->count;i++){
        memset(&v[i],0,sizeof v[i]);v[i].v.ob[0]=p->x[i];v[i].v.ob[1]=y;v[i].v.ob[2]=p->z[i];v[i].v.cn[3]=255;
    }
    unsigned c=p->colour;gDPSetPrimColor(gdl++,0,0,c>>24,(c>>16)&255,(c>>8)&255,c&255);
    gSPVertex(gdl++,v,p->count,0);
    for(unsigned i=1;i+1<p->count;i++)gSP1Triangle(gdl++,0,i,i+1,0);
    return gdl;
}
static Gfx *native_watch_skin_matrix(Gfx *gdl,Mtxf *m){
    Mtx *fixed=dynAllocateMatrix();
    matrix_4x4_7F058C64();matrix_4x4_f32_to_s32(m,fixed);matrix_4x4_7F058C88();
    gSPMatrix(gdl++,fixed,G_MTX_NOPUSH|G_MTX_LOAD|G_MTX_MODELVIEW);return gdl;
}
static Gfx *native_watch_skin_draw(Gfx *gdl,Mtxf *matrices){
    int skin=native_skin_current;if(!skin)return gdl;
    gDPPipeSync(gdl++);gDPSetCycleType(gdl++,G_CYC_1CYCLE);
    gSPClearGeometryMode(gdl++,G_LIGHTING|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR|G_CULL_BOTH|G_FOG);
    gDPSetCombineMode(gdl++,G_CC_PRIMITIVE,G_CC_PRIMITIVE);
    gDPSetRenderMode(gdl++,G_RM_AA_OPA_SURF,G_RM_AA_OPA_SURF2);
    for(int hand=0;hand<3;hand++){
        gdl=native_watch_skin_matrix(gdl,&matrices[hand+1]);
        struct NativeWatchHand p=native_watch_hand(skin,hand,0);
        gdl=native_watch_skin_polygon(gdl,&p,hand==0?5:hand==1?11:2);
    }
    return gdl;
}
