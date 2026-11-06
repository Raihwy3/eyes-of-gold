#include <ultra64.h>
#include "game/player.h"
#include "game/bondview.h"
#include "native_controls.h"
#include "native_settings.h"
#include "native_pad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game/lv.h"
#include "bondconstants.h"
#include "fr.h"
#include "native_test_assert.h"
#include "system.h"
#include "native_demo.h"
static unsigned demo_logs;
void sysLogPrintf(enum LogLevel level,const char *fmt,...){(void)level;(void)fmt;demo_logs++;}
static int current_player;
s32 get_cur_playernum(void){return current_player;}
static struct player player;
struct player *g_CurrentPlayer=&player;
struct player *g_playerPointers[4]={&player,NULL,NULL,NULL};
enum CAMERAMODE g_CameraMode=CAMERAMODE_FP;
static int menu=11,locked,allowed=1,mx,my,reloads;
static int demo_active;
int get_is_ramrom_flag(void){return demo_active;}
static unsigned actions;
static int tx,ty;
static float gy,gp,fy;
void native_input_take_flick(float *yaw){*yaw=fy;fy=0;}
void native_input_take_gyro(float *y,float *p){*y=gy;*p=gp;gy=gp=0;}
void native_input_take_touch_look(int *x,int *y){*x=tx;*y=ty;tx=ty=0;}
int get_currentmenu(void){return menu;}
int lvlGetControlsLockedFlag(void){return locked;}
int disablePlayerActionsWhenPausedOrInMpMenu(void){return allowed;}
void attempt_reload_item_in_hand(s32 hand){assert(hand==GUNRIGHT || hand==GUNLEFT);reloads++;}
void native_input_take_motion(int *x,int *y,unsigned *a){*x=mx;*y=my;*a=actions;actions=0;}
extern int native_game_input_gameplay(void);
s32 g_ClockTimer=1;
void bondviewUpdateSpeedSideways(s32 value){player.speedsideways=value;}
void bondviewUpdateSpeedForwards(s32 value){player.speedforwards=value;}
static void original_movement_commit(struct MoveData moveData) {
#include "native-movement-commit.h"
}
f32 g_GlobalTimerDelta=1.f;
f32 viGetFovY(void){return 60.f;}
#include "native-aim-acceleration.h"
static void original_touch_aim_rates(struct MoveData moveData){
 float ftemp_nostack_sp8C,ftemp_nostack_sp84,ftemp_nostack_sp88,ftemp_nostack_sp98,ftemp_nostack_sp94;
#include "native-touch-aim-axes.h"
}

extern unsigned native_game_render_state(void);
int main(void) {
    struct MoveData m;
    current_player=1;actions=EOG_RELOAD;mx=33;my=44;tx=12;ty=15;
    memset(&m,0,sizeof m);native_controls_apply(&m);
    assert(actions==EOG_RELOAD && tx==12 && ty==15 && !m.analogWalk && !reloads);
    current_player=0;actions=0;mx=my=tx=ty=0;
    struct player idle={0};idle.bonddead=1;g_CurrentPlayer=&idle;
    assert(native_game_input_gameplay());g_CurrentPlayer=&player;
    memset(&player,0,sizeof player);player.crouchpos=CROUCH_STAND;
    native_settings_init(NULL);assert(native_controls_restore(2)==2);
    native_setting_set_style(8);assert(native_controls_restore(2)==8);
    assert(native_game_input_gameplay());
    player.autocrouchpos=CROUCH_STAND;player.fovy=60;
    assert(native_game_render_state()==1);
    player.crouchpos=CROUCH_HALF;assert(native_game_render_state()==3);
    player.insightaimmode=1;assert(native_game_render_state()==7);
    player.crouchpos=CROUCH_STAND;assert(native_game_render_state()==5);
    player.insightaimmode=0;player.fovy=30;assert(native_game_render_state()==5);player.fovy=60;
    player.watch_animation_state=1;assert(!native_game_render_state());player.watch_animation_state=0;
    puts("PASS actual crouch/zoom snapshot: standing/crouch/aim/scoped FOV and watch exclusion");

    memset(&m,0,sizeof m);m.analogTurn=32;m.analogPitch=-27;
    mx=80;my=-70;actions=EOG_RELOAD|EOG_CROUCH|EOG_INTERACT;
    native_controls_apply(&m);
    assert(m.btap);
    assert(m.analogStrafe==75 && m.analogWalk==-65);
    assert(m.analogTurn==32 && m.analogPitch==-27);
    /* Execute the original movement consumer, not merely the input hook. */
    original_movement_commit(m);
    assert(player.speedsideways>1.0f && player.speedforwards<-.9f);
    assert(m.disableLookAhead && m.canLookAhead && m.canTurnTank);
    for(int style=8;style<=9;style++) {
        native_setting_set_style(style);mx=0;my=75;native_controls_apply(&m);assert(!m.btap);
        original_movement_commit(m);assert(player.speedforwards==1 && player.speedsideways==0);
        my=0;mx=-75;native_controls_apply(&m);original_movement_commit(m);
        assert(player.speedforwards==0 && player.speedsideways==-1);
    }
    mx=80;my=-70;
    assert(m.crouchDown && !m.crouchUp && reloads==2);
    player.crouchpos=CROUCH_SQUAT;native_controls_apply(&m);
    assert(!m.crouchDown && !m.crouchUp && reloads==2);
    actions=EOG_CROUCH;native_controls_apply(&m);assert(m.crouchUp && !m.crouchDown);
    g_CameraMode=CAMERAMODE_INTRO;assert(!native_game_input_gameplay());
    g_CameraMode=CAMERAMODE_FP;player.bonddead=1;assert(!native_game_input_gameplay());
    player.bonddead=0;player.watch_animation_state=1;assert(!native_game_input_gameplay());
    player.watch_animation_state=0;menu=10;assert(!native_game_input_gameplay());menu=11;
    locked=1;m.analogWalk=123;actions=EOG_RELOAD;native_controls_apply(&m);
    assert(m.analogWalk==123 && reloads==2);locked=0;
    native_setting_set_style(0);mx=0;my=0;native_controls_apply(&m);assert(m.analogWalk==123);
    memset(&m,0,sizeof m);m.analogWalk=31;m.analogStrafe=-22;
    player.crouchpos=CROUCH_STAND;tx=25;ty=-15;actions=EOG_INTERACT|EOG_RELOAD|EOG_CROUCH;
    native_controls_apply(&m);
    assert(m.analogWalk==31 && m.analogStrafe==-22);
    assert(m.analogTurn==25 && m.analogPitch==-15 && m.btap && m.crouchDown && reloads==4);
    struct MoveData before_touch_aim=m;
    player.insightaimmode=1;player.docentreupdown=1;memset(&m,0,sizeof m);tx=24;ty=-18;
    native_controls_apply(&m);
    assert(m.canNaturalTurn && m.canNaturalPitch && m.disableLookAhead && !player.docentreupdown);
    assert(m.analogTurn==24 && m.analogPitch==-18);
    player.speedtheta=player.speedverta=0;original_touch_aim_rates(m);
    assert(player.speedtheta>0 && player.speedverta>0);
    player.speedtheta=player.speedverta=0;
    memset(&m,0,sizeof m);native_controls_apply(&m);assert(!m.canNaturalTurn && !m.canNaturalPitch);
    player.insightaimmode=0;m=before_touch_aim;
    player.watch_animation_state=1;tx=50;ty=50;actions=EOG_RELOAD;
    native_controls_apply(&m);assert(m.analogTurn==25 && reloads==4 && tx==0 && ty==0);
    player.watch_animation_state=0;native_setting_set_sensitivity(50);
    float yaw=1.f,pitch=-.5f;native_controls_adjust_look(&yaw,&pitch);assert(yaw==.5f && pitch==-.25f);
    native_setting_set_sensitivity(200);yaw=1;pitch=-.5f;native_controls_adjust_look(&yaw,&pitch);assert(yaw==2 && pitch==-1);
    player.watch_animation_state=1;yaw=1;pitch=-.5f;native_controls_adjust_look(&yaw,&pitch);assert(yaw==1 && pitch==-.5f);
    player.watch_animation_state=0;locked=1;native_controls_adjust_look(&yaw,&pitch);assert(yaw==1 && pitch==-.5f);locked=0;
    native_setting_set_sensitivity(100);native_controls_adjust_look(&yaw,&pitch);assert(yaw==1 && pitch==-.5f);
    // Run the original aim-speed easing at both corners, high sensitivity,
    // reversal and release. Consuming a rate must never amplify stored state.
    native_setting_set_sensitivity(200);
    for(int dt=1;dt<=3;dt++) {
        g_GlobalTimerDelta=dt;player.speedtheta=0;
        for(int tick=0;tick<10000;tick++) {
            float input=tick<3000?-1.f:tick<6000?1.f:0.f;
            bondviewCurrentPlayerUpdateSpeedTheta(input);
            float stored=player.speedtheta,consumed=native_controls_look_rate(stored);
            assert(player.speedtheta==stored && __builtin_isfinite(consumed) && __builtin_fabsf(consumed)<=2.f);
            assert(__builtin_fabsf(stored)<=.701f);
        }
        assert(player.speedtheta==0);
    }
    assert(native_controls_look_rate(__builtin_inff())==0 && native_controls_look_rate(__builtin_nanf(""))==0);
    assert(native_controls_look_rate(1e30f)==2 && native_controls_look_rate(-1e30f)==-2);
    native_demo_replay_begin();assert(demo_logs==1);
    native_demo_replay_seed(1,1);assert(demo_logs==1);
    native_demo_replay_seed(1,2);native_demo_replay_seed(1,3);assert(demo_logs==2);
    native_demo_replay_stop(NATIVE_DEMO_TIMER);assert(demo_logs==3);
    demo_active=1;memset(&m,0x5a,sizeof m);struct MoveData recorded=m;
    native_controls_apply(&m);assert(!memcmp(&m,&recorded,sizeof m));
    assert(!native_game_input_gameplay() && native_controls_restore(2)==2);
    assert(native_controls_look_rate(.4f)==.4f);
    demo_active=0;assert(native_game_input_gameplay());
    menu=11;locked=0;allowed=1;player.bonddead=0;player.watch_animation_state=WATCH_ANIMATION_0x0;player.mpmenuon=0;player.fovy=60;
    native_setting_set_gyro(2);native_setting_set_gyro_sensitivity(100);player.vv_theta=180;player.vv_verta=0;gy=2;gp=1;
    native_controls_apply_gyro();assert(player.vv_theta==176 && player.vv_verta==-1);
    native_controls_apply_gyro();assert(player.vv_theta==176 && player.vv_verta==-1);
    native_setting_set_sensitivity(25);gy=2;gp=1;native_controls_apply_gyro();assert(player.vv_theta==172 && player.vv_verta==-2);
    native_setting_set_gyro(1);player.insightaimmode=0;gy=3;gp=3;native_controls_apply_gyro();assert(player.vv_theta==172 && player.vv_verta==-2);
    player.insightaimmode=1;native_setting_set_gyro_sensitivity(200);gy=2;gp=1;native_controls_apply_gyro();assert(player.vv_theta==164 && player.vv_verta==-4);
    locked=1;gy=3;gp=3;native_controls_apply_gyro();assert(player.vv_theta==164 && gy==0 && gp==0);locked=0;
    player.vv_theta=1;player.vv_verta=88;gy=15;gp=-15;native_controls_apply_gyro();assert(player.vv_theta==301 && player.vv_verta==89);
    native_setting_set_gyro(0);
    native_setting_set_style(8);native_setting_set_flick(1);player.vv_theta=350;fy=90;
    native_controls_apply_gyro();assert(player.vv_theta==80 && fy==0);
    native_controls_apply_gyro();assert(player.vv_theta==80);
    locked=1;fy=90;native_controls_apply_gyro();assert(player.vv_theta==80 && fy==0);locked=0;
    menu=1;fy=90;native_controls_apply_gyro();assert(player.vv_theta==80 && fy==0);menu=11;
    native_setting_set_style(9);fy=90;native_controls_apply_gyro();assert(player.vv_theta==80);
    native_setting_set_style(8);native_setting_set_flick(0);fy=90;native_controls_apply_gyro();assert(player.vv_theta==80);
    puts("PASS Flick Stick camera: gyro-off turns, yaw wrap, consume-once and gameplay/style/lock gating");
    puts("PASS camera gyro: independent gain, aim-only, consume-once, locks, yaw wrap and pitch clamp");
    puts("PASS attract playback: recorded MoveData, control style and look speed preserved");
    puts("PASS 30000 original aim acceleration ticks: corner/reverse/release, 200% gain, bounded consumed rate and unchanged stored state");
    puts("PASS look gain: 50/100/200 percent, full-stick speed scaling after curve, watch/locked input excluded");
    puts("PASS touch look preserves legacy movement and ignores watch/control-locked gameplay");
    puts("PASS original MoveData hook separates movement/look, toggles crouch, reloads once and respects menus/death/control locks");
}
void osSyncPrintf(const char *fmt,...) {(void)fmt;}
