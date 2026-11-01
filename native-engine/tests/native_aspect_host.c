#include <ultra64.h>
#include "game/options.h"
#include "game/player.h"
#include "native_aspect.h"
#include "native_settings.h"
extern void portCrosshairTint(s32 *,s32 *,s32 *);
extern void portCrosshairApplyTintCombine(Gfx *);
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "native_test_assert.h"
extern f32 portNativeAspect(void);
static int frontend_menu;
MENU get_currentmenu(void){return (MENU)frontend_menu;}
#include "native-front-aspect.h"

extern s16 bondviewGetCurrentPlayerViewportHeight(void);
extern s16 bondviewGetCurrentPlayerViewportUly(void);
extern void cur_player_set_screen_setting(u32);
static struct player player;
struct player *g_CurrentPlayer=&player;
s32 cameraBufferToggle;
s32 j_text_trigger;
s32 getPlayerCount(void) {return 1;}
s32 get_cur_playernum(void) {return 0;}
static s16 width=320,top,height=240;
s16 viGetX(void){return width;}
s16 viGetY(void){return 240;}
s16 viGetViewLeft(void){return 0;}
s16 viGetViewTop(void){return top;}
s16 viGetViewWidth(void){return width;}
s16 viGetViewHeight(void){return height;}
extern void bgUpdateCurrentPlayerScreenMinMax(void);
int main(void) {
    assert(AA_EN==8);
    native_settings_init(NULL);
    cur_player_set_control_type(2);assert(cur_player_get_control_type()==2);
    native_setting_set_style(8);cur_player_set_control_type(CONTROLLER_CONFIG_HONEY);
    assert(player.cur_player_control_type_0==8 && cur_player_get_control_type()==CONTROLLER_CONFIG_SOLITARE);
    native_setting_set_style(9);cur_player_set_control_type(0);
    assert(player.cur_player_control_type_0==9 && cur_player_get_control_type()==CONTROLLER_CONFIG_SOLITARE);
    native_setting_set_style(0);cur_player_set_control_type(1);
    assert(player.cur_player_control_type_0==0 && cur_player_get_control_type()==0);
    native_settings_init(NULL);
    puts("PASS original control setter honours persisted Vesper/Jinx and original layouts across legacy save restoration");
    Gfx commands[2],untouched[2],expected;
    memset(commands,0xa5,sizeof commands);
    memcpy(untouched,commands,sizeof commands);
    portCrosshairApplyTintCombine(commands);assert(!memcmp(commands,untouched,sizeof commands));
    gDPSetCombineLERP(&expected,ENVIRONMENT,0,TEXEL0_ALPHA,0,TEXEL0,0,ENVIRONMENT,0,
      ENVIRONMENT,0,TEXEL0_ALPHA,0,TEXEL0,0,ENVIRONMENT,0);
    static const int colours[5][3]={{255,255,255},{64,255,64},{255,176,32},{255,48,48},{64,160,255}};
    for(int i=1;i<6;i++) {
        s32 r,g,b;native_setting_set_crosshair(i);portCrosshairTint(&r,&g,&b);
        assert(r==colours[i-1][0] && g==colours[i-1][1] && b==colours[i-1][2]);
        portCrosshairApplyTintCombine(commands);assert(!memcmp(&commands[1],&expected,sizeof expected));
        assert(!memcmp(commands,untouched,sizeof(Gfx)));
    }
    native_setting_set_crosshair(0);
    puts("PASS crosshair palettes use the texture alpha silhouette and leave the environment command intact");
    for(int full=0;full<3;full++) {
        native_setting_set_aspect(full);
        for(frontend_menu=MENU_LEGAL_SCREEN;frontend_menu<MENU_MAX;frontend_menu++) {
            int folder=(frontend_menu>=MENU_FILE_SELECT && frontend_menu<=MENU_BRIEFING) ||
                       (frontend_menu>=MENU_MISSION_FAILED && frontend_menu<=MENU_NO_CONTROLLERS);
            float aspect=native_frontend_aspect(4.f/3.f);
            assert(fabsf(aspect-(folder?4.f/3.f:native_aspect_ratio(full)))<1.e-6f);
            if(folder)for(int x=-2;x<=2;x++) {
                // An authored 3D folder point and its overlaid 2D label
                // must retain the same horizontal position after output scaling.
                float original=x*.2f;
                float projected=original*(4.f/3.f)/aspect;
                float mesh=(projected+1)*native_aspect_width(full)*.5f;
                float label=(original+1)*native_aspect_width(full)*.5f;
                assert(fabsf(mesh-label)<.0001f);
            }
        }
    }
    native_settings_init(NULL);
    puts("PASS actual frontend aspect hook keeps folder mesh/text registration in all three outputs; intro/game projection follows selection");
    native_setting_set_aspect(2);assert(fabsf(portNativeAspect()-1.25f)<1.e-6f && native_aspect_width(2)==300);native_settings_init(NULL);
    for(unsigned i=0;i<20;++i) {
        SCREEN_RATIO_OPTION option=(SCREEN_RATIO_OPTION)(i&1);
        set_screen_ratio(option);
        assert(get_screen_ratio()==option);
        assert(fabsf(portNativeAspect()-(i&1?5.f/3.f:4.f/3.f))<1.e-6f);
        assert(fabsf(portNativeAspect()*240-native_aspect_width(i&1))<.001f);
        assert(!strcmp(native_aspect_label(i&1),i&1?"5:3\n":"4:3\n"));
    }
    for(int i=0;i<2;++i) {
        width=i?400:320;
        bgUpdateCurrentPlayerScreenMinMax();
        assert(player.screensize.min.x==0 && player.screensize.min.y==0);
        assert(player.screensize.max.x==width && player.screensize.max.y==240);
    }
    top=30;height=180;bgUpdateCurrentPlayerScreenMinMax();
    assert(player.screensize.min.y==30 && player.screensize.max.y==210);
    puts("PASS room clipping reaches every viewport edge in both aspect ratios without a one-pixel sky border");
    cur_player_set_screen_setting(SCREEN_SIZE_FULLSCREEN);
    assert(bondviewGetCurrentPlayerViewportHeight()==240);
    assert(bondviewGetCurrentPlayerViewportUly()==0);
    cur_player_set_screen_setting(SCREEN_SIZE_WIDESCREEN);
    assert(bondviewGetCurrentPlayerViewportHeight()==180);
    assert(bondviewGetCurrentPlayerViewportUly()==30);
    cur_player_set_screen_setting(SCREEN_SIZE_CINEMA);
    assert(bondviewGetCurrentPlayerViewportHeight()==136);
    assert(bondviewGetCurrentPlayerViewportUly()==52);
    puts("PASS original Full viewport uses all 240 LCD rows; Wide/Cinema keep their deliberate framing");
    puts("PASS original watch ratio state switches native projection between 320x240 (4:3) and 400x240 (5:3)");
}
void osSyncPrintf(const char *fmt,...) {(void)fmt;}
