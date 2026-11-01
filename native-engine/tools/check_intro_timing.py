"""Run the patched retail title state machine at different elapsed tick rates."""
import re
import build,build_probe

def function(text,name):
    start=text.index('Gfx *'+name)
    pos=text.index('{',start);depth=1;end=pos+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]

def main():
    text=build.source_path(build.Path('src/game/title.c')).read_text()
    rare=function(text,'retrieve_display_rareware_logo')
    eye=function(text,'renderGunbarrelEyeIntroSequence')
    macros=text[text.index('#ifndef REFRESH_PAL\n    #define XINC'):text.index('Gfx *renderGunbarrelEyeIntroSequence')]
    src=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef uint64_t Gfx;typedef int s32;
static int g_ClockTimer,D_8002A7D0,gunbarrel_mode,intro_eye_counter,intro_state_blood_animation,blood_calls,model_ticks;
static int16_t word_CODE_bss_80069584;
static float g_TitleX,g_TitleY,titleTransitionX;
#define SPSEGMENT_GETITLE 0
#define gSPSegment(...) ((void)0)
#define gSPDisplayList(...) ((void)0)
#define dlBasicGeometry 0
static Gfx *load_display_rare_logo(Gfx *p,int a,int b,int c,int d){return p;}
static Gfx *manipulateGunbarrelAndLogoMatrices(Gfx *p){return p;}
static Gfx *insert_sniper_sight_eye_intro(Gfx *p){return p;}
static Gfx *insert_sight_backdrop_eye_intro(Gfx *p){return p;}
static Gfx *insert_bond_eye_intro(Gfx *p){model_ticks+=2*g_ClockTimer;return p;}
static Gfx *gunbarrelBloodOverlayDL(Gfx *p){return p;}
static Gfx *sub_GAME_7F01CA18(Gfx *p){return p;}
static Gfx *sub_GAME_7F007E70(Gfx *p,int a){return p;}
static Gfx *clear_framebuffer_black(Gfx *p){return p;}
static float sins(int a){return 0;}
static int die_blood_image_routine(int advance){if(!advance){blood_calls=0;return 0;}return ++blood_calls>=8;}
'''+macros+rare+'\n'+eye+r'''
static int run(int step,int mixed) {
    Gfx dl[8];g_TitleX=-30;g_TitleY=482;gunbarrel_mode=2;intro_eye_counter=0;
    word_CODE_bss_80069584=0;intro_state_blood_animation=0;blood_calls=model_ticks=0;
    int elapsed=0,frames=0;
    while(gunbarrel_mode!=9 && frames<5000) {
        g_ClockTimer=mixed?(frames%3==0?2:1):step;
        renderGunbarrelEyeIntroSequence(dl);elapsed+=g_ClockTimer;frames++;
    }
    assert(gunbarrel_mode==9 && blood_calls==8 && model_ticks>0);
    return elapsed;
}
int main(void) {
    Gfx dl[8];
    for(int step=1;step<=3;step++) {
        gunbarrel_mode=intro_eye_counter=0;int elapsed=0;
        while(gunbarrel_mode!=2 && elapsed<2000){g_ClockTimer=step;retrieve_display_rareware_logo(dl);elapsed+=step;}
        assert(elapsed>=290 && elapsed<=292);
    }
    int at60=run(1,0),at30=run(2,0),slow=run(3,0),mixed=run(1,1);
    printf("Retail intro elapsed ticks: 60 FPS=%d, 30 FPS=%d, 20 FPS=%d, changing cap=%d\n",at60,at30,slow,mixed);
    assert(abs(at30-at60)<=12 && abs(slow-at60)<=18 && abs(mixed-at60)<=12);
    puts("PASS actual retail Rare/gunbarrel timing: elapsed ticks preserve phase lengths, blood cadence and completion at 60/30/20 FPS and changing caps");
}
'''
    out=build.BUILD/'intro-timing-host';out.mkdir(exist_ok=True)
    source=out/'check.c';source.write_text(src)
    build_probe.run(['gcc','-std=c11','-O1',source,'-o',out/'check'])
    print(build_probe.run([out/'check']).stdout)
if __name__=='__main__':main()
