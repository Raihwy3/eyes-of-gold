#include "native_dynamic_lod.h"
#include "native_vignette.h"
static void check_dynamic_lod_and_vignette(void) {
    for(int fps=30;fps<=60;fps+=30) {
        NativeDynamicLod controller={0};uint64_t now=1;
        for(unsigned i=0;i<fps*6u;i++){now+=2000000u/fps;native_dynamic_lod_step(&controller,now,80000,fps,1,1);}
        assert(controller.strength==3);
        for(unsigned i=0;i<fps*4u;i++){now+=1000000u/fps;native_dynamic_lod_step(&controller,now,1000000u/fps,fps,1,1);}
        assert(controller.strength==3);
        for(unsigned i=0;i<fps*8u;i++){now+=1000000u/fps;native_dynamic_lod_step(&controller,now,500000u/fps,fps,1,1);}
        assert(controller.strength==1);
        native_dynamic_lod_step(&controller,now+1000000,1000000,fps,1,0);assert(controller.strength==1);
        native_dynamic_lod_step(&controller,now+2000000,1000000,fps,1,1);assert(controller.strength==1);
        native_dynamic_lod_step(&controller,now+3000000,1000000,fps,0,1);assert(!controller.enabled);
    }
    for(unsigned state=0;state<8;state++)for(int mode=0;mode<4;mode++) {
        int expected=mode==0 || ((state&1) && (mode==1?(state&2)!=0:mode==2?(state&4)!=0:(state&6)!=0));
        assert(native_vignette_active(mode,state)==expected);
    }
    puts("PASS Dynamic LOD and conditional vignette: 30/60 FPS feedback, bounded strengths, slow headroom recovery, pause/hitch rejection and all activation combinations");
}
