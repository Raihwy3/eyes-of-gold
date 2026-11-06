/* HID sampling and original N64 pad state, with two optional native layouts. */
#include <3ds.h>
#include "native_input.h"
#include "native_pad.h"
#include "native_gyro.h"
#include "native_flick.h"
#include "native_settings.h"
#include "native_bottom.h"
#include "native_menu_focus.h"
_Static_assert((unsigned)KEY_A==(unsigned)EOG_A && (unsigned)KEY_Y==(unsigned)EOG_Y && (unsigned)KEY_ZR==(unsigned)EOG_ZR,"HID key contract");
extern int native_game_input_gameplay(void) __attribute__((weak));
static LightLock input_lock;
static struct NativeGyro gyro;
static struct NativeFlick flick;
void native_input_take_flick(float *yaw){LightLock_Lock(&input_lock);*yaw=flick.pending;flick.pending=0;LightLock_Unlock(&input_lock);}
static int gyro_enabled,gyro_failed;
static float gyro_coefficient;
extern unsigned native_game_render_state(void) __attribute__((weak));
void native_input_take_gyro(float *yaw,float *pitch){LightLock_Lock(&input_lock);*yaw=gyro.yaw;*pitch=gyro.pitch;gyro.yaw=gyro.pitch=0;LightLock_Unlock(&input_lock);}
static struct NativePad pad;
static unsigned pending_actions,last_actions;
static unsigned menu_buttons;
static unsigned pressed;
static int touch_look_x,touch_look_y;
void native_input_take_touch_look(int *x,int *y){LightLock_Lock(&input_lock);*x=touch_look_x;*y=touch_look_y;touch_look_x=touch_look_y=0;LightLock_Unlock(&input_lock);}
void native_input_press(unsigned buttons){LightLock_Lock(&input_lock);pressed|=buttons;LightLock_Unlock(&input_lock);}
unsigned native_input_menu_buttons(void){LightLock_Lock(&input_lock);unsigned v=menu_buttons;LightLock_Unlock(&input_lock);return v;}
static int exit_requested,suspended;


void native_input_init(void) {flick=(struct NativeFlick){0};gyro=(struct NativeGyro){0};gyro_enabled=gyro_failed=0;gyro_coefficient=0;LightLock_Init(&input_lock);pad=(struct NativePad){0};touch_look_x=touch_look_y=0;pending_actions=last_actions=menu_buttons=pressed=0;exit_requested=suspended=0;}
void native_input_suspend(void) {LightLock_Lock(&input_lock);suspended=1;flick=(struct NativeFlick){0};gyro=(struct NativeGyro){0};if(gyro_enabled)HIDUSER_DisableGyroscope();gyro_enabled=0;LightLock_Unlock(&input_lock);}
void inputUpdate(void) {
    LightLock_Lock(&input_lock);
    if(suspended){LightLock_Unlock(&input_lock);return;}
    hidScanInput();
    u32 held=hidKeysHeld();circlePosition circle,cstick;
    menu_buttons=held;
    hidCircleRead(&circle);hidCstickRead(&cstick);
    int gameplay=native_game_input_gameplay?native_game_input_gameplay():0;
    int mode=native_setting_gyro();
    if(mode && !gyro_enabled && !gyro_failed){
        gyro_enabled=R_SUCCEEDED(HIDUSER_EnableGyroscope());
        if(!gyro_enabled || R_FAILED(HIDUSER_GetGyroscopeRawToDpsCoefficient(&gyro_coefficient)) || !(gyro_coefficient>0.f)){
            if(gyro_enabled)HIDUSER_DisableGyroscope();
            gyro_enabled=0;gyro_failed=1;
        }
    }
    if(!mode){if(gyro_enabled)HIDUSER_DisableGyroscope();gyro_enabled=gyro_failed=0;}
    angularRate rate={0};if(gyro_enabled)hidGyroRead(&rate);
    unsigned render_state=native_game_render_state?native_game_render_state():(gameplay?1u:0u);
    int aiming=(render_state&4) || (held&KEY_L);
    uint64_t now=(uint64_t)(svcGetSystemTick()*(1000000.0/SYSCLOCK_ARM11));
    /* Pitch is rotation about the handheld's horizontal axis; yaw about up. */
    native_gyro_sample(&gyro,now,gyro_enabled?rate.z/gyro_coefficient:0,gyro_enabled?rate.x/gyro_coefficient:0,
        gyro_enabled && gameplay && (render_state&1) && (mode==2 || aiming));
    int dx=0,dy=0;unsigned touch_buttons=0,touch_actions=0;
    if(!gameplay && native_bottom_menu_stick)native_bottom_menu_stick(circle.dx,circle.dy);
    if(native_bottom_input) {
        touchPosition point={0};hidTouchRead(&point);
        held=native_bottom_input(held,(held&KEY_TOUCH)!=0,point.px,point.py,gameplay,&dx,&dy,&touch_buttons,&touch_actions);
        menu_buttons=held;
    }
    if(native_bottom_seed_active && native_bottom_seed_active()){circle.dx=circle.dy=cstick.dx=cstick.dy=0;}
    int use_flick=native_setting_flick() && native_setting_style()==8 && gameplay && (render_state&1);
    native_flick_sample(&flick,now,cstick.dx,cstick.dy,use_flick);
    pad=native_pad_map(held,circle.dx,circle.dy,cstick.dx,cstick.dy,native_setting_style(),gameplay);
    if(use_flick)pad.x=pad.y=0;
    pad.buttons|=touch_buttons;pad.actions|=touch_actions;
    if(gameplay) {
        touch_look_x=touch_look_x+dx>80?80:touch_look_x+dx< -80?-80:touch_look_x+dx;
        touch_look_y=touch_look_y+dy>80?80:touch_look_y+dy< -80?-80:touch_look_y+dy;
    } else touch_look_x=touch_look_y=0;
    if(gameplay)pending_actions|=pad.actions&~last_actions;
    else pending_actions=0;
    last_actions=pad.actions;
    if((held&(KEY_SELECT|KEY_START))==(KEY_SELECT|KEY_START))exit_requested=1;
    LightLock_Unlock(&input_lock);
}
int inputConnectedMask(void){return 1;}
unsigned inputComputePad(int index,signed char *x,signed char *y) {
    LightLock_Lock(&input_lock);
    unsigned state=index==0?(pad.buttons|pressed):0;
    if(index==0)pressed=0;
    if(x)*x=index==0?pad.x:0;
    if(y)*y=index==0?pad.y:0;
    LightLock_Unlock(&input_lock);return state;
}
void native_input_take_motion(int *x,int *y,unsigned *actions) {
    LightLock_Lock(&input_lock);
    *x=pad.move_x;*y=pad.move_y;*actions=pending_actions;pending_actions=0;
    LightLock_Unlock(&input_lock);
}
int native_input_exit_requested(void) {
    LightLock_Lock(&input_lock);int requested=exit_requested;LightLock_Unlock(&input_lock);return requested;
}
int inputRumbleSupported(int index){(void)index;return 0;}
void inputRumble(int index,float strength,float time){(void)index;(void)strength;(void)time;}
