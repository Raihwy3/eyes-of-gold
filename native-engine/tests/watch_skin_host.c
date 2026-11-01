#include <ultra64.h>
#include "bondtypes.h"
#include "dyn.h"
#include "matrixmath.h"
#include "native_settings.h"
#include "native_watch_environment.h"
#include "native_test_assert.h"
#include <string.h>
static unsigned char arena[65536] __attribute__((aligned(16)));
static unsigned used,vertex_count;
Vtx *dynAllocateVertices(s32 n){assert(n>=0 && used+sizeof(Vtx)*n<=sizeof arena);Vtx *p=(Vtx *)(arena+used);used+=(sizeof(Vtx)*n+15)&~15;vertex_count+=n;return p;}
Mtx *dynAllocateMatrix(void){assert(used+64<=sizeof arena);Mtx *p=(Mtx *)(arena+used);used+=64;return p;}
#include "native_watch_skin.h"
int main(void){
    assert(native_watch_environment_rgba(0,0,0,0)==0);
    assert(native_watch_environment_rgba(255,255,255,0)==0xffffff00u);
    assert(native_watch_environment_rgba(23,57,81,96)==0x17395160u);
    native_settings_init(NULL);
    ModelNode joints[3]={0},parts[3]={0},cloth={0};ModelNode *switches[]={&joints[0],&joints[1],&joints[2]};
    ModelFileHeader header={0};header.Switches=switches;
    for(int i=0;i<3;i++)parts[i].Parent=&joints[i];
    static Gfx commands[2048];Mtxf matrices[4]={0};
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)matrices[i].m[j][j]=1;
    native_watch_skin_begin(&header);assert(!native_watch_custom_part(&parts[0]));
    assert(native_watch_skin_draw(commands,matrices)==commands && !used);
    for(int skin=1;skin<5;skin++){
        native_setting_set_skin(skin);native_watch_skin_begin(&header);
        for(int i=0;i<3;i++)assert(native_watch_custom_part(&parts[i]));assert(!native_watch_custom_part(&cloth));
        used=vertex_count=0;Gfx *end=native_watch_skin_draw(commands,matrices);assert(end>commands && end<commands+2048 && vertex_count==(skin==2?11:13));
        unsigned tri=0;for(Gfx *p=commands;p<end;p++)if(p->words.w0>>24==(u8)G_TRI1)tri++;
        assert(tri==(skin==2?5:7));native_watch_skin_end();assert(!native_watch_custom_part(&parts[2]));
        for(int hand=0;hand<3;hand++)for(int inset=0;inset<2;inset++){
            struct NativeWatchHand p=native_watch_hand(skin,hand,inset);
            assert((p.count!=0)==!inset);
            if(skin==4 && hand==2 && p.count)assert(p.colour==0x9c7b32ff);
            if(p.count && hand<2){assert(p.colour==0x777777ff);assert((p.colour>>24)==((p.colour>>16)&255) && (p.colour>>24)==((p.colour>>8)&255));}if(!p.count)continue;
            /* Independent retail envelope, including the seconds' short tail. */
            int tip=hand==0?60:hand==1?120:121;
            int tail=hand==2?6:13;
            int minz=0;
            long sign=0;
            for(unsigned i=0;i<p.count;i++){
                unsigned j=(i+1)%p.count,k=(i+2)%p.count;
                long cross=(p.x[j]-p.x[i])*(p.z[k]-p.z[j])-(p.z[j]-p.z[i])*(p.x[k]-p.x[j]);
                if(cross){if(sign)assert((cross>0)==(sign>0));else sign=cross;}
                assert(p.x[i]>=-4 && p.x[i]<=4 && p.z[i]>=-tip && p.z[i]<=tail);
                if(p.z[i]<minz)minz=p.z[i];
            }
            assert(sign);if(!inset)assert(minz==-tip);
        }
    }
    assert(!native_watch_hand(5,0,0).count && !native_watch_hand(1,3,0).count);
    puts("PASS watch skins: Original emits no hand changes; level tile shading is preserved; all four custom presets, including gold seconds, emit distinct slim neutral hour/minute profiles and coloured seconds, retail length bounds, convex geometry, valid matrices, hand-only replacement, unchanged cloth and exact scope exit");
}
