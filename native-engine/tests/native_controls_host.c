#include <ultra64.h>
#include "game/player.h"
#include "game/gun.h"
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
s32 g_PlayerIsInTank;
f32 g_TankTurretAngle, g_TankTurretOrientationAngleRad, g_TankTurretOrientationAngleDeg;
struct player *g_playerPointers[4]={&player,NULL,NULL,NULL};
enum CAMERAMODE g_CameraMode=CAMERAMODE_FP;
static int menu=11,locked,allowed=1,mx,my,reloads;
static int demo_active;
int get_is_ramrom_flag(void){return demo_active;}
static int scoped_weapon;
ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand){(void)hand;return ITEM_SNIPERRIFLE;}
u32 bondwalkItemCheckBitflags(ITEM_IDS item,u32 mask){(void)item;return scoped_weapon?mask:0;}
static unsigned actions,held_buttons;
unsigned native_input_menu_buttons(void){return held_buttons;}
static int tx,ty;
f32 getPlayer_c_screenwidth(void){return 320;}
f32 getPlayer_c_screenheight(void){return 240;}
static float gy,gp,fy;
void native_input_take_flick(float *yaw){*yaw=fy;fy=0;}
void native_input_take_gyro(float *y,float *p){*y=gy;*p=gp;gy=gp=0;}
void native_input_take_touch_look(int *x,int *y){*x=tx;*y=ty;tx=ty=0;}
int get_currentmenu(void){return menu;}
int lvlGetControlsLockedFlag(void){return locked;}
int disablePlayerActionsWhenPausedOrInMpMenu(void){return allowed;}
void attempt_reload_item_in_hand(GUNHAND hand){assert(hand==GUNRIGHT || hand==GUNLEFT);reloads++;}
void native_input_take_motion(int *x,int *y,unsigned *a){*x=mx;*y=my;*a=actions;actions=0;}
extern int native_game_input_gameplay(void);
s32 g_ClockTimer=1;
void bondviewUpdateSpeedSideways(s32 value){player.speedsideways=value;}
void bondviewUpdateSpeedForwards(s32 value){player.speedforwards=value;}
static void original_movement_commit(struct MoveData moveData) {
#include "native-movement-commit.h"
}
#include "native-lean-target.h"
static void original_lean_commit(struct MoveData moveData){
#include "native-lean-consumer.h"
}
f32 g_GlobalTimerDelta=1.f;
f32 viGetFovY(void){return 60.f;}
#include "native-aim-acceleration.h"
static void original_touch_aim_rates(struct MoveData moveData){
 float ftemp_nostack_sp8C,ftemp_nostack_sp84,ftemp_nostack_sp88,ftemp_nostack_sp98,ftemp_nostack_sp94;
#include "native-touch-aim-axes.h"
}

extern unsigned native_game_render_state(void);
/* Compare the generated pipeline against the upstream stick-only pipeline. */
static WeaponStats pipeline_weapon;
WeaponStats *get_ptr_item_statistics(ITEM_IDS item){(void)item;return &pipeline_weapon;}
f32 getPlayer_c_screenleft(void){return 0;}
f32 getPlayer_c_screentop(void){return 0;}
void transformAndNormalizeByLength2Dto3D(struct coord2d *in,coord3d *out,f32 length){(void)in;(void)length;memset(out,0,sizeof *out);}
void sub_GAME_7F067AB4(coord3d *coords){(void)coords;}
int reference_motion_disabled(float *x,float *y){(void)x;(void)y;return 0;}
#include "native-reticle-pipeline.h"
static void check_reticle_pipeline(void){
    struct player saved=player,reference;
    for(int gyro=0;gyro<=2;gyro++)for(int weapon=0;weapon<3;weapon++){
        native_setting_set_style(8);native_setting_set_aim_lock(0);native_setting_set_gyro(gyro);
        player=(struct player){0};player.fovy=60;player.crouchpos=CROUCH_STAND;
        player.insightaimmode=0;float ax=0,ay=0;native_controls_aim_turn(&ax,&ay);
        player.insightaimmode=1;player.vv_theta=180;reference=player;
        pipeline_weapon.CrosshairSpeed=weapon==0?.8f:weapon==1?.945f:.65f;
        pipeline_weapon.AimLockSpeed=.9f;
        for(int n=0;n<600;n++){
            g_ClockTimer=n%5;float x=.65f*sinf(n*.071f),y=.65f*cosf(n*.053f);
            gy=gp=0;sub_GAME_7F067FBC(x,y);
            g_CurrentPlayer=&reference;reference_manual(x,y);g_CurrentPlayer=&player;
            assert(player.guncrossdamp==pipeline_weapon.CrosshairSpeed);
            assert(player.crosshair_x_pos==reference.crosshair_x_pos && player.crosshair_y_pos==reference.crosshair_y_pos);
            assert(fabsf(player.crosshair_angle.x-reference.crosshair_angle.x)<.001f);
            assert(fabsf(player.crosshair_angle.y-reference.crosshair_angle.y)<.001f);
            assert(player.gun_azimuth_angle==reference.gun_azimuth_angle);
            assert(fabsf(player.field_FFC.x-reference.field_FFC.x)<.001f);
            assert(fabsf(player.field_FFC.y-reference.field_FFC.y)<.001f);
            assert(fabsf(player.vv_theta-180)<.001f && fabsf(player.vv_verta)<.001f);
        }
    }
    for(int gyro=0;gyro<=2;gyro++){
        native_setting_set_gyro(gyro);native_setting_set_style(8);native_setting_set_aim_lock(0);
        player=(struct player){0};player.fovy=60;
        float ax=0,ay=0;native_controls_aim_turn(&ax,&ay);
        player.insightaimmode=1;reference=player;
        pipeline_weapon.CrosshairSpeed=.8f;pipeline_weapon.AimLockSpeed=.9f;
        g_ClockTimer=1;unsigned quadrants=0;int crossings=0;float last_y=0;
        for(int n=0;n<1440;n++){
            float angle=n*.01745329252f;
            struct NativePad sample=native_pad_map(EOG_L,0,0,(int)lroundf(156*cosf(angle)),(int)lroundf(156*sinf(angle)),8,1);
            float x=sample.x*.65f/80,y=sample.y*.65f/80;
            gy=gp=0;sub_GAME_7F067FBC(x,y);
            g_CurrentPlayer=&reference;reference_manual(x,y);g_CurrentPlayer=&player;
            assert(fabsf(player.crosshair_angle.x-reference.crosshair_angle.x)<.001f);
            assert(fabsf(player.crosshair_angle.y-reference.crosshair_angle.y)<.001f);
            float rx=player.crosshair_angle.x-160,ry=player.crosshair_angle.y-120;
            if(n>360){
                quadrants|=1u<<((rx>0?1:0)+(ry>0?2:0));
                if(last_y<0 && ry>=0 && rx>0)crossings++;
                assert(rx*rx+ry*ry>2500);
            }
            last_y=ry;
        }
        assert(quadrants==15 && crossings==3);
    }
    puts("PASS real mapped C-stick circles match 0.9.5 stock pipeline point-for-point: four quadrants, four continuous revolutions, gyro Off/Aim/Look+Aim");
    player=(struct player){0};player.fovy=60;player.insightaimmode=0;
    float ax=0,ay=0;native_controls_aim_turn(&ax,&ay);
    player.insightaimmode=1;player.vv_theta=180;native_setting_set_gyro(2);native_setting_set_gyro_sensitivity(100);
    g_ClockTimer=1;gy=-1;gp=0;sub_GAME_7F067FBC(0,0);
    assert(player.crosshair_angle.x>160 && player.crosshair_x_pos==0 && player.vv_theta==180);
    assert(player.guncrossdamp==pipeline_weapon.CrosshairSpeed);
    float held=player.crosshair_angle.x;gy=gp=0;sub_GAME_7F067FBC(0,0);
    assert(fabsf(player.crosshair_angle.x-held)<.001f);
    player=saved;player.insightaimmode=0;ax=ay=0;native_controls_aim_turn(&ax,&ay);
    puts("PASS 5400 actual original/generated reticle comparisons: stock stick and weapon damping unchanged with gyro Off/Aim/Look+Aim; motion offsets move and hold without changing stick state");
}
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
    native_setting_set_gyro(0);player.insightaimmode=1;player.docentreupdown=1;
    memset(&m,0,sizeof m);tx=24;ty=-18;
    float aim_theta=player.vv_theta,aim_pitch=player.vv_verta;
    native_controls_apply(&m);
    assert(!m.canNaturalTurn && !m.canNaturalPitch && m.disableLookAhead && !player.docentreupdown);
    assert(!m.analogTurn && !m.analogPitch);
    float touch_ax=0,touch_ay=0;assert(native_controls_aim_turn(&touch_ax,&touch_ay));
    assert(fabsf(touch_ax-.05f)<.00001f && fabsf(touch_ay-.05f)<.00001f && player.vv_theta==aim_theta && player.vv_verta==aim_pitch);
    float hold_ax=touch_ax,hold_ay=touch_ay;
    memset(&m,0,sizeof m);native_controls_apply(&m);touch_ax=touch_ay=0;
    assert(native_controls_aim_turn(&touch_ax,&touch_ay));
    assert(fabsf(touch_ax-hold_ax)<.00001f && fabsf(touch_ay-hold_ay)<.00001f);
    current_player=1;touch_ax=touch_ay=0;assert(!native_controls_aim_turn(&touch_ax,&touch_ay));current_player=0;
    touch_ax=touch_ay=0;assert(native_controls_aim_turn(&touch_ax,&touch_ay));assert(fabsf(touch_ax-hold_ax)<.00001f);
    player.insightaimmode=0;memset(&m,0,sizeof m);native_controls_apply(&m);
    player.insightaimmode=1;touch_ax=touch_ay=0;assert(!native_controls_aim_turn(&touch_ax,&touch_ay));
    native_setting_set_aim_lock(1);memset(&m,0,sizeof m);tx=24;ty=-18;native_controls_apply(&m);
    assert(m.canNaturalTurn && m.canNaturalPitch && m.analogTurn==24 && m.analogPitch==-18);
    player.speedtheta=player.speedverta=0;original_touch_aim_rates(m);
    assert(player.speedtheta>0 && player.speedverta>0);
    player.speedtheta=player.speedverta=0;native_setting_set_aim_lock(0);
    player.insightaimmode=0;m=before_touch_aim;
    player.watch_animation_state=1;tx=50;ty=50;actions=EOG_RELOAD;
    native_controls_apply(&m);assert(m.analogTurn==25 && reloads==4 && tx==0 && ty==0);
    player.watch_animation_state=0;native_setting_set_sensitivity(50);native_setting_set_vertical_sensitivity(50);
    float yaw=1.f,pitch=-.5f;native_controls_adjust_look(&yaw,&pitch);assert(yaw==.5f && pitch==-.25f);
    native_setting_set_sensitivity(200);native_setting_set_vertical_sensitivity(200);yaw=1;pitch=-.5f;native_controls_adjust_look(&yaw,&pitch);assert(yaw==2 && pitch==-1);
    player.watch_animation_state=1;yaw=1;pitch=-.5f;native_controls_adjust_look(&yaw,&pitch);assert(yaw==1 && pitch==-.5f);
    player.watch_animation_state=0;locked=1;native_controls_adjust_look(&yaw,&pitch);assert(yaw==1 && pitch==-.5f);locked=0;
    native_setting_set_sensitivity(100);native_setting_set_vertical_sensitivity(100);native_controls_adjust_look(&yaw,&pitch);assert(yaw==1 && pitch==-.5f);
    // Run the original aim-speed easing at both corners, high sensitivity,
    // reversal and release. Consuming a rate must never amplify stored state.
    native_setting_set_sensitivity(1000);
    for(int dt=1;dt<=3;dt++) {
        g_GlobalTimerDelta=dt;player.speedtheta=0;
        for(int tick=0;tick<10000;tick++) {
            float input=tick<3000?-1.f:tick<6000?1.f:0.f;
            bondviewCurrentPlayerUpdateSpeedTheta(input);
            float stored=player.speedtheta,consumed=native_controls_look_rate(stored);
            assert(player.speedtheta==stored && __builtin_isfinite(consumed) && __builtin_fabsf(consumed)<=10.f);
            assert(__builtin_fabsf(stored)<=.701f);
        }
        assert(player.speedtheta==0);
    }
    assert(native_controls_look_rate(__builtin_inff())==0 && native_controls_look_rate(__builtin_nanf(""))==0);
    assert(native_controls_look_rate(1e30f)==10 && native_controls_look_rate(-1e30f)==-10);
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
    assert(native_setting_gyro_sensitivity()==250);
    native_setting_set_gyro(2);player.vv_theta=180;player.vv_verta=0;gy=gp=1;
    native_controls_apply_gyro();assert(player.vv_theta==177.5f && player.vv_verta==-2.5f);
    native_setting_set_gyro(2);native_setting_set_gyro_sensitivity(100);player.vv_theta=180;player.vv_verta=0;gy=2;gp=1;
    native_controls_apply_gyro();assert(player.vv_theta==178 && player.vv_verta==-1);
    native_controls_apply_gyro();assert(player.vv_theta==178 && player.vv_verta==-1);
    native_setting_set_sensitivity(25);gy=2;gp=1;native_controls_apply_gyro();assert(player.vv_theta==176 && player.vv_verta==-2);
    native_setting_set_gyro(1);player.insightaimmode=0;gy=3;gp=3;native_controls_apply_gyro();assert(player.vv_theta==176 && player.vv_verta==-2);
    player.insightaimmode=1;native_setting_set_gyro_sensitivity(200);gy=2;gp=1;
    float ax=0,ay=0;assert(native_controls_aim_turn(&ax,&ay));
    assert(ax<0 && ay>0 && player.vv_theta==176 && player.vv_verta==-2 && gy==0 && gp==0);
    float held_x=ax,held_y=ay;ax=ay=0;assert(native_controls_aim_turn(&ax,&ay));
    assert(fabsf(ax-held_x)<.00001f && fabsf(ay-held_y)<.00001f);
    native_controls_apply_gyro();assert(player.vv_theta==176 && player.vv_verta==-2);
    for(int i=0;i<10;i++){gy=-10;gp=0;ax=ay=0;assert(native_controls_aim_turn(&ax,&ay));assert(ax<=.65f && ax>=-.65f);}
    assert(player.vv_theta!=176);
    float edge_theta=player.vv_theta;ax=ay=0;native_controls_aim_turn(&ax,&ay);assert(player.vv_theta==edge_theta);
    locked=1;gy=3;gp=3;ax=ay=0;assert(!native_controls_aim_turn(&ax,&ay));
    native_controls_apply_gyro();assert(player.vv_theta==edge_theta && gy==0 && gp==0);locked=0;
    player.insightaimmode=0;native_setting_set_gyro(2);
    player.vv_theta=1;player.vv_verta=88;gy=15;gp=-15;native_controls_apply_gyro();assert(player.vv_theta==331 && player.vv_verta==89);
    native_setting_set_gyro_sensitivity(100);player.insightaimmode=0;player.vv_theta=180;player.vv_verta=0;gy=gp=1;
    native_controls_apply_gyro();assert(player.vv_theta==179 && player.vv_verta==-1);
    scoped_weapon=1;player.insightaimmode=1;native_setting_set_style(8);held_buttons=EOG_DUP;memset(&m,0,sizeof m);native_controls_apply(&m);
    assert(m.zooming && m.zoomInFovPersec==2 && !m.zoomOutFovPersec);
    held_buttons=EOG_DDOWN;memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.zoomOutFovPersec==2 && !m.zoomInFovPersec);
    native_setting_set_style(9);held_buttons=EOG_X;memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.zoomInFovPersec==2 && !m.analogPitch);
    held_buttons=EOG_B;memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.zoomOutFovPersec==2 && !m.analogPitch);
    scoped_weapon=0;held_buttons=EOG_X;memset(&m,0,sizeof m);m.analogPitch=40;native_controls_apply(&m);assert(m.analogPitch==40);
    held_buttons=0;native_setting_set_aim_lock(1);gy=gp=1;ax=ay=0;float before=player.vv_theta;
    assert(native_controls_aim_turn(&ax,&ay));assert(!ax && !ay && player.vv_theta==before-1);
    native_setting_set_aim_lock(0);player.insightaimmode=0;
    player.fovy=7;player.headlook.f[0]=.2f;player.headlook.f[1]=.3f;player.headlook.f[2]=.8f;
    player.headup.f[0]=.2f;player.headup.f[1]=.8f;player.headup.f[2]=.3f;
    native_controls_scope_head();assert(player.headlook.f[0]==0 && player.headlook.f[2]==1 && player.headup.f[1]==1);
    player.fovy=60;
    for(int style=8;style<=9;style++){
        native_setting_set_style(style);native_setting_set_aim_movement(1);
        player.insightaimmode=1;player.crouchpos=CROUCH_STAND;scoped_weapon=0;held_buttons=0;
        mx=75;my=-70;memset(&m,0,sizeof m);m.digitalStepForward=m.digitalStepBack=1;
        native_controls_apply(&m);
        assert(m.analogWalk==0 && m.analogStrafe==0 && !m.digitalStepForward && !m.digitalStepBack && !m.digitalStepLeft && !m.digitalStepRight && m.rLeanRight && !m.rLeanLeft);
        assert(m.crouchDown && !m.crouchUp);
        original_movement_commit(m);assert(player.speedforwards==0 && player.speedsideways==0);
        original_lean_commit(m);assert(player.swaytarget==75.f);
        mx=-75;memset(&m,0,sizeof m);native_controls_apply(&m);
        assert(m.rLeanLeft && !m.rLeanRight && !m.analogStrafe && !m.analogWalk);
        original_lean_commit(m);assert(player.swaytarget==-75.f);
        mx=0;memset(&m,0,sizeof m);native_controls_apply(&m);
        original_lean_commit(m);assert(player.swaytarget==0);
        mx=30;memset(&m,0,sizeof m);native_controls_apply(&m);assert(!m.rLeanLeft && !m.rLeanRight);
        mx=-30;memset(&m,0,sizeof m);native_controls_apply(&m);assert(!m.rLeanLeft && !m.rLeanRight);
        mx=75;
        my=70;memset(&m,0,sizeof m);native_controls_apply(&m);assert(!m.analogWalk && m.crouchUp && !m.crouchDown);
        my=0;memset(&m,0,sizeof m);native_controls_apply(&m);assert(!m.crouchDown && !m.crouchUp);
        scoped_weapon=1;my=70;memset(&m,0,sizeof m);native_controls_apply(&m);
        assert(!m.analogWalk && m.zoomInFovPersec>0 && !m.zoomOutFovPersec && !m.crouchDown && !m.crouchUp);
        my=-70;memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.zoomOutFovPersec>0 && !m.zoomInFovPersec);
        scoped_weapon=0;player.crouchpos=CROUCH_SQUAT;player.insightaimmode=0;my=70;
        memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.analogWalk==65 && !m.crouchUp && !m.crouchDown && !m.rLeanLeft && !m.rLeanRight);
        original_lean_commit(m);assert(player.swaytarget==0);
        player.insightaimmode=1;native_setting_set_aim_movement(0);memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.analogWalk==65);
        native_setting_set_aim_movement(1);g_PlayerIsInTank=1;memset(&m,0,sizeof m);native_controls_apply(&m);assert(m.analogWalk==65);g_PlayerIsInTank=0;
    }
    native_setting_set_aim_movement(0);player.insightaimmode=0;mx=my=0;
    for(int style=8;style<=9;style++){
        native_setting_set_style(style);g_PlayerIsInTank=1;mx=75;my=-70;memset(&m,0,sizeof m);
        m.analogTurn=40;m.analogPitch=-30;native_controls_apply(&m);
        assert(m.analogTurn==70 && m.analogWalk==-65 && m.analogStrafe==40 && m.analogPitch==-30);
        assert(m.canNaturalTurn && m.canTurnTank && m.canLookAhead);
        player.vv_theta=100;gy=10;gp=5;native_controls_apply_gyro();assert(player.vv_theta==100);
        float tax=0,tay=0;player.insightaimmode=1;native_setting_set_gyro(1);
        assert(!native_controls_aim_turn(&tax,&tay));
        g_TankTurretAngle=1.0f;player.vv_verta=0;gy=5;gp=2;
        native_controls_apply_gyro();
        assert(g_TankTurretAngle!=1.0f && player.vv_verta!=0);
        player.insightaimmode=0;native_setting_set_gyro(0);
        g_PlayerIsInTank=0;mx=my=0;
    }
    puts("PASS tank controls: Circle Pad drives and steers hull, C-stick turns and pitches turret, gyro driving bypass and aim mode gyro support");
    puts("PASS classic aim movement: Vesper/Jinx original lean targets, no walking/strafe, directional crouch/stand, neutral stance, scoped zoom, release/free movement and tank bypass");
    puts("PASS gyro reticle: centred camera, retained target, original aim-path handoff, bounded edge overflow and no idle edge drift");
    native_setting_set_gyro(0);
    native_setting_set_style(8);native_setting_set_flick(1);player.vv_theta=350;fy=90;
    native_controls_apply_gyro();assert(player.vv_theta==80 && fy==0);
    native_controls_apply_gyro();assert(player.vv_theta==80);
    locked=1;fy=90;native_controls_apply_gyro();assert(player.vv_theta==80 && fy==0);locked=0;
    menu=1;fy=90;native_controls_apply_gyro();assert(player.vv_theta==80 && fy==0);menu=11;
    native_setting_set_style(9);fy=90;native_controls_apply_gyro();assert(player.vv_theta==80);
    native_setting_set_style(8);native_setting_set_flick(0);fy=90;native_controls_apply_gyro();assert(player.vv_theta==80);
    native_setting_set_sensitivity(250);native_setting_set_vertical_sensitivity(100);
    assert(native_controls_look_rate(.4f)==1.f && fabsf(native_controls_pitch_rate(.4f)-.4f)<.00001f);
    native_setting_set_gyro(0);player.insightaimmode=1;float sx=.2f,sy=.2f;
    assert(!native_controls_aim_turn(&sx,&sy));assert(fabsf(sx-.2f)<.00001f && fabsf(sy-.2f)<.00001f);
    player.insightaimmode=0;
    puts("PASS independent free-look axes; original aiming input remains unscaled");
    float aim_reference[20][2];
    for(int persistent=0;persistent<=1;persistent++){
        native_setting_set_always_crosshair(persistent);native_setting_set_aim_lock(0);native_setting_set_gyro(2);
        native_setting_set_sensitivity(250);native_setting_set_vertical_sensitivity(250);
        player.insightaimmode=0;float ax=0,ay=0;native_controls_aim_turn(&ax,&ay);
        player.insightaimmode=1;player.guncrossdamp=.01f;player.crosshair_x_pos=player.crosshair_y_pos=0;
        player.vv_theta=180;player.vv_verta=0;
        for(int n=0;n<20;n++){
            gy=n<10?.1f:-.1f;gp=n<10?.05f:-.05f;ax=ay=0;
            assert(native_controls_aim_turn(&ax,&ay));
            if(!persistent){aim_reference[n][0]=ax;aim_reference[n][1]=ay;}
            else assert(fabsf(ax-aim_reference[n][0])<.00001f && fabsf(ay-aim_reference[n][1])<.00001f);
        }
        assert(!native_setting_aim_lock());
    }
    check_reticle_pipeline();
    puts("PASS Always Crosshair leaves the moving gyro reticle trajectory and Aim Lock setting unchanged");
    puts("PASS Flick Stick camera: gyro-off turns, yaw wrap, consume-once and gameplay/style/lock gating");
    puts("PASS camera gyro: independent gain, aim-only, consume-once, locks, yaw wrap and pitch clamp");
    puts("PASS attract playback: recorded MoveData, control style and look speed preserved");
    puts("PASS 30000 original aim acceleration ticks: corner/reverse/release, new 400% maximum (10x gain), bounded consumed rate and unchanged stored state");
    puts("PASS look gain: 50/100/200 percent, full-stick speed scaling after curve, watch/locked input excluded");
    puts("PASS touch look preserves legacy movement and ignores watch/control-locked gameplay");
    puts("PASS original MoveData hook separates movement/look, toggles crouch, reloads once and respects menus/death/control locks");
}
void osSyncPrintf(const char *fmt,...) {(void)fmt;}
