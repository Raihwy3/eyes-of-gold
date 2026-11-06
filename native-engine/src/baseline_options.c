/* Original defaults plus native display and crosshair option hooks. */
#include <ultra64.h>
#include "frontoptions.h"
#include "game/options.h"
#include "native_aspect.h"
#include "native_detail.h"
#include "native_settings.h"
#include "native_controls.h"
extern SCREEN_RATIO_OPTION get_screen_ratio(void) __attribute__((weak));
f32 portScreenShakeScale=1.0f;
s32 portSkipIntro=0,portNoHitFlash=0,portCrosshairHide=0,portAllUnlocked=0;
f32 portDrawDistanceMultiplier(void){return 1.0f;}
f32 portFogDistanceMultiplier(void){return 1.0f;}
f32 portLodDistanceMultiplier(void){return native_detail_lod_scale(native_setting_lod_strength());}
f32 portFovYScaleFactor(void){return 1.0f;}
f32 portRoomPoolScale(void){return 1.0f;}
f32 portNativeAspect(void) {
    /* Standalone diagnostics have no watch/options module. */
    int chosen=native_setting_aspect();
    if(chosen>=0)return native_aspect_ratio(chosen);
    return get_screen_ratio?native_aspect_ratio(get_screen_ratio()!=0):0.0f;
}
f32 portScaleFovY(f32 value,s32 title){(void)title;return value;}
s32 portHudScalePercent(void){return 100;}
int portCrosshairPersistent(void){return 0;}
int portCrosshairStyle(void){return 0;}
f32 portCrosshairScale(void){return 1.0f;}
void portCrosshairTint(s32 *r,s32 *g,s32 *b) {
    static const u8 palette[7][3]={{255,255,255},{255,255,255},{64,255,64},{255,176,32},{255,48,48},{64,160,255},{192,96,255}};
    int colour=native_setting_crosshair();*r=palette[colour][0];*g=palette[colour][1];*b=palette[colour][2];
}
void portCrosshairApplyTintCombine(Gfx *command) {
    if(!command || !native_setting_crosshair())return;
    gDPSetCombineLERP(command+1,
      ENVIRONMENT,0,TEXEL0_ALPHA,0,TEXEL0,0,ENVIRONMENT,0,
      ENVIRONMENT,0,TEXEL0_ALPHA,0,TEXEL0,0,ENVIRONMENT,0);
}
f32 portGunSwayTheta(f32 elapsed){(void)elapsed;return 0.0f;}
int portMouseAimPdGetTurn(f32 *x,f32 *y){return native_controls_aim_turn(x,y);}
int portAIWideView(void){return 0;}
Gfx *optionsFileSelectLabel(Gfx *commands){return commands;}
