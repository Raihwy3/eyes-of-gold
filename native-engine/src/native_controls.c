/* Narrow input hook: original movement, collision, weapons and aiming stay active. */
#include <ultra64.h>
#include "game/player.h"
#include "game/gun.h"
#include "game/options.h"
#include "game/bondview.h"
#include "game/lv.h"
#include "native_controls.h"
#include "native_input.h"
#include "native_pad.h"
#include "native_settings.h"
#include "native_demo.h"
#include "system.h"
#include <math.h>
static unsigned native_demo_seed_drift;
void native_demo_replay_begin(void) {
    native_demo_seed_drift=0;
    sysLogPrintf(LOG_NOTE,"Attract replay started");
}
void native_demo_replay_seed(unsigned expected,unsigned actual) {
    if(expected==actual)return;
    if(!native_demo_seed_drift++)sysLogPrintf(LOG_NOTE,
        "Attract replay RNG drift: recorded=%u actual=%u; continuing recorded input",expected,actual);
}
void native_demo_replay_stop(int reason) {
    const char *why=reason==NATIVE_DEMO_CHECKSUM?"packet checksum":reason==NATIVE_DEMO_INPUT?"physical input":reason==NATIVE_DEMO_END?"end of recording":"recorded time limit";
    sysLogPrintf(LOG_NOTE,"Attract replay exit: %s; RNG drift packets=%u",why,native_demo_seed_drift);
}
extern int get_currentmenu(void);
extern int lvlGetControlsLockedFlag(void);
extern int disablePlayerActionsWhenPausedOrInMpMenu(void);
extern void attempt_reload_item_in_hand(GUNHAND hand);
extern void native_input_take_motion(int *,int *,unsigned *) __attribute__((weak));
extern void native_bottom_game_tick(void) __attribute__((weak));
int native_controls_restore(int original) {
    if(native_demo_active())return original;
    int selected=native_setting_style();return selected>=0?selected:original;
}
static struct player *native_controlled_player(void){return g_playerPointers[0];}
int native_game_input_gameplay(void) {
    if(native_demo_active())return 0;
    struct player *controlled=native_controlled_player();
    return get_currentmenu()==11 && controlled && !controlled->bonddead &&
           (g_CameraMode==CAMERAMODE_NONE || g_CameraMode==CAMERAMODE_FP || g_CameraMode==CAMERAMODE_MP) &&
           controlled->watch_animation_state==WATCH_ANIMATION_0x0 && !controlled->mpmenuon;
}
unsigned native_game_render_state(void) {
    struct player *controlled=native_controlled_player();
    if(!native_game_input_gameplay() || lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu())return 0;
    int crouch=controlled->crouchpos<CROUCH_STAND || controlled->autocrouchpos<CROUCH_STAND;
    int zoom=controlled->insightaimmode || (controlled->fovy>0 && controlled->fovy<59.f);
    return 1u|(crouch?2u:0u)|(zoom?4u:0u);
}
static int crouch_target=CROUCH_STAND;
static int legacy_touch_crouch,classic_aim_previous;
static int gyro_aim_active,touch_aim_active;
static int touch_aim_dx,touch_aim_dy;
static float gyro_aim_x,gyro_aim_y;
static int safe_axis(int v){return v< -5?v+5:v>5?v-5:0;}
void native_controls_apply(struct MoveData *m) {
    if(native_demo_active())return;
    /* Only player 1 owns the physical/touch controls; never drain for P2. */
    if(get_cur_playernum()!=0)return;
    if(!native_game_input_gameplay() || !g_CurrentPlayer->insightaimmode ||
       lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu()){
        gyro_aim_active=touch_aim_active=0;touch_aim_dx=touch_aim_dy=0;
    }

    if(native_bottom_game_tick && get_currentmenu()==11)native_bottom_game_tick();
    if(!native_input_take_motion)return;
    int x,y;unsigned actions;native_input_take_motion(&x,&y,&actions);
    int touch_x=0,touch_y=0,modern=native_setting_style()>=8;
    if(native_input_take_touch_look)native_input_take_touch_look(&touch_x,&touch_y);
    if(!modern && !actions && !touch_x && !touch_y && !legacy_touch_crouch && !(native_setting_aim_lock() && g_CurrentPlayer->insightaimmode))return;
    if(!native_game_input_gameplay() || g_CurrentPlayer->bonddead ||
       lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu()) {
        crouch_target=g_CurrentPlayer?g_CurrentPlayer->crouchpos:CROUCH_STAND;return;
    }
    int classic_aim=modern && native_setting_aim_movement() &&
                    g_CurrentPlayer->insightaimmode && g_PlayerIsInTank!=1;
    // Keep the stance selected with the stick when aim is released. Avoid
    // the native toggle's cached target pulling Bond back to his old height.
    if(classic_aim || classic_aim_previous)crouch_target=g_CurrentPlayer->crouchpos;
    classic_aim_previous=classic_aim;
    /* Keep a press captured between game ticks, and consume it once.
     * The original btap consumer handles doors/panels and tank entry/exit. */
    if(modern || (actions&EOG_INTERACT))m->btap=(actions&EOG_INTERACT)!=0;
    if(g_CurrentPlayer->insightaimmode && !native_setting_aim_lock()){
        // Reticle deltas are consumed once by the original gun aiming hook.
        // Keep the target after the finger stops; only edge overflow pans.
        if(touch_x || touch_y){
            touch_aim_active=1;
            touch_aim_dx+=touch_x;touch_aim_dy+=touch_y;
            if(touch_aim_dx>160)touch_aim_dx=160;else if(touch_aim_dx< -160)touch_aim_dx=-160;
            if(touch_aim_dy>160)touch_aim_dy=160;else if(touch_aim_dy< -160)touch_aim_dy=-160;
            m->disableLookAhead=1;g_CurrentPlayer->docentreupdown=0;
        }
    }else{
        if(touch_x)m->canNaturalTurn=1;
        if(touch_y){m->canNaturalPitch=1;m->disableLookAhead=1;g_CurrentPlayer->docentreupdown=0;}
        m->analogTurn+=touch_x;m->analogPitch+=touch_y;
    }
    if(m->analogTurn>80)m->analogTurn=80;else if(m->analogTurn< -80)m->analogTurn=-80;
    if(m->analogPitch>80)m->analogPitch=80;else if(m->analogPitch< -80)m->analogPitch=-80;
    if(modern && g_CurrentPlayer->insightaimmode && bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT),WEAPONSTATBITFLAG_DISABLE_CROUCH)){
        unsigned held=native_input_menu_buttons();
        int zoom_in=(held&(native_setting_style()==8?EOG_DUP:EOG_X))!=0;
        int zoom_out=(held&(native_setting_style()==8?EOG_DDOWN:EOG_B))!=0;
        m->zooming=1;m->zoomInFovPersec=zoom_in && !zoom_out?2.f:0.f;
        m->zoomOutFovPersec=zoom_out && !zoom_in?2.f:0.f;
        if(native_setting_style()==9 && (zoom_in || zoom_out)){m->analogPitch=0;m->speedVertaUp=m->speedVertaDown=0;}
    }
    if(native_setting_aim_lock() && g_CurrentPlayer->insightaimmode){
        m->canNaturalTurn=m->canNaturalPitch=1;m->disableLookAhead=1;
        m->aimTurnLeftSpeed=m->aimTurnRightSpeed=m->speedVertaUp=m->speedVertaDown=0;
    }
    if(modern) {
    m->analogStrafe=safe_axis(x);m->analogWalk=safe_axis(y);
    m->rLeanLeft=m->rLeanRight=0;
    m->digitalStepLeft=m->digitalStepRight=m->digitalStepForward=m->digitalStepBack=0;
    /* These poorly named original flags also enable analog walking/strafe.
     * disableLookAhead separately suppresses automatic camera levelling. */
    m->disableLookAhead=1;m->canLookAhead=1;m->canTurnTank=1;
    }
    /* Do not let a camera stick also trigger the original aim+crouch shortcut. */
    if(actions&EOG_CROUCH) {
        if(!modern)crouch_target=g_CurrentPlayer->crouchpos;
        crouch_target=crouch_target==CROUCH_STAND?CROUCH_SQUAT:CROUCH_STAND;
        legacy_touch_crouch=!modern;
    }
    if(modern || legacy_touch_crouch) {
        m->crouchDown=g_CurrentPlayer->crouchpos>crouch_target;
        m->crouchUp=g_CurrentPlayer->crouchpos<crouch_target;
        if(!m->crouchDown && !m->crouchUp)legacy_touch_crouch=0;
    }
    if(classic_aim){
        m->analogWalk=m->analogStrafe=0;
        m->digitalStepLeft=m->digitalStepRight=m->digitalStepForward=m->digitalStepBack=0;
        // Use the original lean flags: the engine offsets the upper body and
        // camera through its existing sway/collision path, without strafing.
        m->rLeanLeft=x< -30;m->rLeanRight=x>30;
        // Vertical input uses the original crouch/stand flags and gate.
        if(bondwalkItemCheckBitflags(getCurrentPlayerWeaponId(GUNRIGHT),WEAPONSTATBITFLAG_DISABLE_CROUCH)){
            m->crouchDown=m->crouchUp=0;
            if(y>30 || y< -30){
                float rate=(y<0?-y:y)/70.f;if(rate>1)rate=1;
                m->zooming=1;m->zoomInFovPersec=y>30?2.f*rate:0;
                m->zoomOutFovPersec=y< -30?2.f*rate:0;
            }
        }else if(!(actions&EOG_CROUCH)){
            m->crouchDown=y< -30;m->crouchUp=y>30;
        }
    }
    if(actions&EOG_RELOAD) {
        attempt_reload_item_in_hand(GUNRIGHT);
        attempt_reload_item_in_hand(GUNLEFT);
    }
}

/* Scale the consumed rate, never the persistent acceleration state. The
 * original aiming path eases that state toward a target on subsequent ticks. */
float native_controls_look_rate(float rate) {
    if(!__builtin_isfinite(rate))return 0.f;
    if(!native_game_input_gameplay() || lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu())return rate;
    if(rate>1.f)rate=1.f;else if(rate< -1.f)rate=-1.f;
    return rate*(native_setting_sensitivity()*.01f);
}
float native_controls_pitch_rate(float rate) {
    float scaled=native_controls_look_rate(rate);
    if(!native_game_input_gameplay() || lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu())return scaled;
    return scaled*(float)native_setting_vertical_sensitivity()/native_setting_sensitivity();
}
void native_controls_adjust_look(float *yaw,float *pitch) {
    *yaw=native_controls_look_rate(*yaw);*pitch=native_controls_pitch_rate(*pitch);
}

/* Original gunfire calls this AFTER its stick/weapon damping. Inputs are
 * normalized stock reticle positions; add motion without changing that filter. */
int native_controls_aim_turn(float *x,float *y){
 if(get_cur_playernum()!=0)return 0;
 if( (!native_setting_gyro() && !native_setting_aim_lock() && !touch_aim_active) ||
    !native_game_input_gameplay() || !g_CurrentPlayer->insightaimmode ||
    lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu()) {gyro_aim_active=touch_aim_active=0;touch_aim_dx=touch_aim_dy=0;return 0;}
 float yaw=0,pitch=0;if(native_input_take_gyro)native_input_take_gyro(&yaw,&pitch);
 if(!__builtin_isfinite(yaw) || !__builtin_isfinite(pitch))yaw=pitch=0;
 float gain=native_setting_gyro_sensitivity()*.01f;
 float fov=g_CurrentPlayer->fovy;
 if(!(fov>1 && fov<=120))fov=60;
 if(fov<60)gain*=fov/60;
 float height=getPlayer_c_screenheight(),width=getPlayer_c_screenwidth();
 float aspect=height>0 && width>0?width/height:1.3333333f;
 float vertical=tanf(fov*.00872664626f),horizontal=vertical*aspect;
 if(native_setting_aim_lock()){
   g_CurrentPlayer->vv_theta-=yaw*gain;g_CurrentPlayer->vv_verta-=pitch*gain;
   while(g_CurrentPlayer->vv_theta<0)g_CurrentPlayer->vv_theta+=360;
   while(g_CurrentPlayer->vv_theta>=360)g_CurrentPlayer->vv_theta-=360;
   if(g_CurrentPlayer->vv_verta>89)g_CurrentPlayer->vv_verta=89;
   if(g_CurrentPlayer->vv_verta< -89)g_CurrentPlayer->vv_verta=-89;
   gyro_aim_active=0;*x=*y=0;return 1;
 }
 if(!gyro_aim_active){
   gyro_aim_x=g_CurrentPlayer->crosshair_x_pos*(1-g_CurrentPlayer->guncrossdamp)-*x;
   gyro_aim_y=g_CurrentPlayer->crosshair_y_pos*(1-g_CurrentPlayer->guncrossdamp)-*y;
   if(!__builtin_isfinite(gyro_aim_x))gyro_aim_x=0;
   if(!__builtin_isfinite(gyro_aim_y))gyro_aim_y=0;
   gyro_aim_active=1;
 }
 // Touch look is expressed in legacy analog units (three per scaled
 // pixel). Undo that conversion before moving a screen-space reticle.
 gyro_aim_x+=width>0?2.f*touch_aim_dx/(3.f*width):0;
 gyro_aim_y-=height>0?2.f*touch_aim_dy/(3.f*height):0;
 touch_aim_dx=touch_aim_dy=0;
 gyro_aim_x-=yaw*gain*.01745329252f/horizontal;
 gyro_aim_y+=pitch*gain*.01745329252f/vertical;
 float tx=gyro_aim_x+*x,ty=gyro_aim_y+*y;
 float cx=tx< -.65f?-.65f:tx>.65f?.65f:tx;
 float cy=ty< -.65f?-.65f:ty>.65f?.65f:ty;
 /* Only motion beyond the aiming area turns the camera. Subtract that
  * overflow so a stationary device does not keep dragging the view. */
 g_CurrentPlayer->vv_theta+=(tx-cx)*horizontal*57.29577951f;
 g_CurrentPlayer->vv_verta-=(ty-cy)*vertical*57.29577951f;
 while(g_CurrentPlayer->vv_theta<0)g_CurrentPlayer->vv_theta+=360;
 while(g_CurrentPlayer->vv_theta>=360)g_CurrentPlayer->vv_theta-=360;
 if(g_CurrentPlayer->vv_verta>89)g_CurrentPlayer->vv_verta=89;
 if(g_CurrentPlayer->vv_verta< -89)g_CurrentPlayer->vv_verta=-89;
 gyro_aim_x=cx-*x;gyro_aim_y=cy-*y;
 *x=cx;*y=cy;
 return 1;
}

/* Applied once after the original camera integrators, before their final clamp.
 * This bypasses stick acceleration and stick sensitivity intentionally. */
void native_controls_apply_gyro(void){
 if(get_cur_playernum()!=0)return;
 float yaw=0,pitch=0,flick=0;
 if(native_input_take_gyro)native_input_take_gyro(&yaw,&pitch);
 if(native_input_take_flick)native_input_take_flick(&flick);
 if(!native_game_input_gameplay() || lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu())return;
 if(native_setting_flick() && native_setting_style()==8 && __builtin_isfinite(flick)) {
   if(flick>360)flick=360;else if(flick< -360)flick=-360;
   g_CurrentPlayer->vv_theta+=flick;
   while(g_CurrentPlayer->vv_theta<0)g_CurrentPlayer->vv_theta+=360;
   while(g_CurrentPlayer->vv_theta>=360)g_CurrentPlayer->vv_theta-=360;
 }
 int mode=native_setting_gyro();
 if(g_CurrentPlayer->insightaimmode)return;
 gyro_aim_active=0;
 if(!mode || (mode==1 && !g_CurrentPlayer->insightaimmode))return;
 if(!__builtin_isfinite(yaw) || !__builtin_isfinite(pitch))return;
 float gain=native_setting_gyro_sensitivity()*.01f;
 /* Match the game's FOV scaling while zoomed, including the sniper scope. */
 float fov=g_CurrentPlayer->fovy;
 if(fov>0 && fov<60)gain*=fov/60.f;
 g_CurrentPlayer->vv_theta-=yaw*gain;g_CurrentPlayer->vv_verta-=pitch*gain;
 while(g_CurrentPlayer->vv_theta<0)g_CurrentPlayer->vv_theta+=360;
 while(g_CurrentPlayer->vv_theta>=360)g_CurrentPlayer->vv_theta-=360;
 if(g_CurrentPlayer->vv_verta>89)g_CurrentPlayer->vv_verta=89;
 if(g_CurrentPlayer->vv_verta< -89)g_CurrentPlayer->vv_verta=-89;
}

/* Fade animation-induced scope drift between 50 and 20 degrees, as in Midas.
 * Recorded demos must keep their original camera basis. */
void native_controls_scope_head(void){
 if(native_demo_active() || !native_setting_steady_scope() || !g_CurrentPlayer || g_CurrentPlayer->bonddead)return;
 float k=(g_CurrentPlayer->fovy-20.f)/30.f;
 if(!__builtin_isfinite(k) || k>=1)return;
 if(k<0)k=0;
 g_CurrentPlayer->headlook.f[0]*=k;g_CurrentPlayer->headlook.f[1]*=k;
 g_CurrentPlayer->headlook.f[2]=g_CurrentPlayer->headlook.f[2]*k+1-k;
 g_CurrentPlayer->headup.f[0]*=k;g_CurrentPlayer->headup.f[2]*=k;
 g_CurrentPlayer->headup.f[1]=g_CurrentPlayer->headup.f[1]*k+1-k;
}
