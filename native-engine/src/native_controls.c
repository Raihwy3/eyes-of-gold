/* Narrow input hook: original movement, collision, weapons and aiming stay active. */
#include <ultra64.h>
#include "game/player.h"
#include "game/options.h"
#include "game/bondview.h"
#include "game/lv.h"
#include "native_controls.h"
#include "native_input.h"
#include "native_pad.h"
#include "native_settings.h"
#include "native_demo.h"
#include "system.h"
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
extern void attempt_reload_item_in_hand(s32 hand);
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
static int legacy_touch_crouch;
static int safe_axis(int v){return v< -5?v+5:v>5?v-5:0;}
void native_controls_apply(struct MoveData *m) {
    if(native_demo_active())return;
    /* Only player 1 owns the physical/touch controls; never drain for P2. */
    if(get_cur_playernum()!=0)return;
    if(native_bottom_game_tick && get_currentmenu()==11)native_bottom_game_tick();
    if(!native_input_take_motion)return;
    int x,y;unsigned actions;native_input_take_motion(&x,&y,&actions);
    int touch_x=0,touch_y=0,modern=native_setting_style()>=8;
    if(native_input_take_touch_look)native_input_take_touch_look(&touch_x,&touch_y);
    if(!modern && !actions && !touch_x && !touch_y && !legacy_touch_crouch)return;
    if(!native_game_input_gameplay() || g_CurrentPlayer->bonddead ||
       lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu()) {
        crouch_target=g_CurrentPlayer?g_CurrentPlayer->crouchpos:CROUCH_STAND;return;
    }
    /* Keep a press captured between game ticks, and consume it once.
     * The original btap consumer handles doors/panels and tank entry/exit. */
    if(modern || (actions&EOG_INTERACT))m->btap=(actions&EOG_INTERACT)!=0;
    /* Manual aim normally disables these analog camera axes. Touch dragging
     * must remain a look input while aiming, without replacing reticle/stick
     * input or letting automatic pitch centring cancel a vertical drag. */
    if(touch_x)m->canNaturalTurn=1;
    if(touch_y){m->canNaturalPitch=1;m->disableLookAhead=1;g_CurrentPlayer->docentreupdown=0;}
    m->analogTurn+=touch_x;m->analogPitch+=touch_y;
    if(m->analogTurn>80)m->analogTurn=80;else if(m->analogTurn< -80)m->analogTurn=-80;
    if(m->analogPitch>80)m->analogPitch=80;else if(m->analogPitch< -80)m->analogPitch=-80;
    if(modern) {
    m->analogStrafe=safe_axis(x);m->analogWalk=safe_axis(y);
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
void native_controls_adjust_look(float *yaw,float *pitch) {
    *yaw=native_controls_look_rate(*yaw);*pitch=native_controls_look_rate(*pitch);
}

/* Applied once after the original camera integrators, before their final clamp.
 * This bypasses stick acceleration and stick sensitivity intentionally. */
void native_controls_apply_gyro(void){
 if(get_cur_playernum()!=0 || !native_input_take_gyro)return;
 float yaw=0,pitch=0;native_input_take_gyro(&yaw,&pitch);
 int mode=native_setting_gyro();
 if(!mode || !native_game_input_gameplay() || lvlGetControlsLockedFlag() || !disablePlayerActionsWhenPausedOrInMpMenu())return;
 if(mode==1 && !g_CurrentPlayer->insightaimmode)return;
 if(!__builtin_isfinite(yaw) || !__builtin_isfinite(pitch))return;
 float gain=native_setting_gyro_sensitivity()*.01f;
 /* Match the game's FOV scaling while zoomed, including the sniper scope. */
 float fov=g_CurrentPlayer->fovy;
 if(fov>0 && fov<60)gain*=fov/60.f;
 g_CurrentPlayer->vv_theta-=yaw*gain*2.f;g_CurrentPlayer->vv_verta-=pitch*gain;
 while(g_CurrentPlayer->vv_theta<0)g_CurrentPlayer->vv_theta+=360;
 while(g_CurrentPlayer->vv_theta>=360)g_CurrentPlayer->vv_theta-=360;
 if(g_CurrentPlayer->vv_verta>89)g_CurrentPlayer->vv_verta=89;
 if(g_CurrentPlayer->vv_verta< -89)g_CurrentPlayer->vv_verta=-89;
}
