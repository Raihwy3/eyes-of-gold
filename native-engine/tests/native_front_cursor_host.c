#include "front.c"
#include "native_test_assert.h"
#include <math.h>
extern void portCrosshairTint(s32 *,s32 *,s32 *);
static struct sImageTableEntry sight,icons[8];
struct sImageTableEntry *crosshairimage=&sight,*mainfolderimages=icons;
static int colour[4];
static struct sImageTableEntry *bound;
static unsigned quits;
static int changed_row=-1,changed_direction;
void native_menu_option_change(int row,int direction){changed_row=row;changed_direction=direction;}
static uint64_t test_now;
uint64_t sysGetMicroseconds(void){return test_now;}
unsigned native_input_menu_buttons(void){return EOG_A;}
void native_game_request_quit(void){quits++;}
f32 floorFloat(f32 x){return floorf(x);}
void texSelect(Gfx **g,struct sImageTableEntry *t,u32 a,s32 b,u32 c){(void)g;(void)a;(void)b;(void)c;bound=t;}
void display_image_at_position(Gfx **g,f32 *xy,f32 *half,s32 w,s32 h,s32 a,s32 b,s32 c,s32 r,s32 green,s32 blue,s32 alpha,s32 format,s32 d) {
    (void)xy;(void)half;(void)w;(void)h;(void)a;(void)b;(void)c;(void)format;(void)d;
    colour[0]=r;colour[1]=green;colour[2]=blue;colour[3]=alpha;
    gDPSetEnvColor((*g)++,r,green,blue,alpha);
    gDPSetCombineMode((*g)++,G_CC_MODULATEIA,G_CC_MODULATEIA);
}
int main(void) {
    native_settings_init(NULL);
    sight.width=sight.height=32;
    for(int i=0;i<6;i++) {
        native_setting_set_crosshair(i);folder_selection_screen_option_icon=0;
        Gfx commands[4],original;
        assert(frontDrawCursor(commands)==commands+2 && bound==&sight);
        s32 r,g,b;portCrosshairTint(&r,&g,&b);
        assert(colour[0]==r && colour[1]==g && colour[2]==b && colour[3]==220);
        gDPSetCombineMode(&original,G_CC_MODULATEIA,G_CC_MODULATEIA);
        assert((memcmp(commands+1,&original,sizeof original)!=0)==(i!=0));
        for(int icon=1;icon<=2;icon++) {
            folder_selection_screen_option_icon=icon;
            assert(frontDrawCursor(commands)==commands+2);
            assert(bound==icons+(icon==1?IMG_COPY:IMG_DEL));
            assert(colour[0]==255 && colour[1]==255 && colour[2]==255 && colour[3]==220);
            assert(!memcmp(commands+1,&original,sizeof original));
        }
    }
    puts("PASS actual frontend cursor: all six saved crosshair palettes, alpha-mask tinting and unchanged copy/erase icons");
    for(int menu=MENU_FILE_SELECT;menu<=MENU_BRIEFING;menu++) {
        current_menu=menu;
        assert(native_front_available()==(menu==MENU_MODE_SELECT));
    }
    current_menu=MENU_MODE_SELECT;
    for(int cheats=0;cheats<2;cheats++) {
        is_cheat_menu_available=cheats;
        int count=cheats?5:4,first=cheats?3:2;
        cursor_h_pos=200;
        for(int row=0;row<count;row++) {
            cursor_v_pos=native_mode_y(row)+6;
            assert(native_mode_y(row)+14<330);
            assert(native_mode_row()==row);
            assert(native_mode_extra()==(row>=first?row-first:-1));
        }
        cursor_v_pos=native_mode_y(first)+6;
        native_front_state.open=0;native_mode_activate();assert(native_front_state.open && !quits);
        assert(native_front_options_blocked());test_now+=999999;assert(native_front_options_blocked());
        test_now++;assert(!native_front_options_blocked());
        native_front_close_options();assert(!native_front_state.open && native_front_options_blocked());
        test_now+=1000000;
        assert(native_front_transition_input_blocked(EOG_B,0,0));
        assert(native_front_transition_input_blocked(0,60,0));
        assert(native_front_transition_input_blocked(0,0,0));
        assert(!native_front_transition_input_blocked(0,0,0));
        cursor_v_pos=native_mode_y(first+1)+6;
        native_mode_activate();assert(quits==1);quits=0;
        cursor_h_pos=TABS_LEFT_EDGE+1;cursor_v_pos=PREVTAB_TAB_TOP+10;
        assert(native_mode_extra()==-1);
    }
    native_front_open_options();test_now+=1000000;
    assert(native_front_category==-1 && native_front_option_count()==NATIVE_MENU_CATEGORY_COUNT);
    for(int category=0;category<NATIVE_MENU_CATEGORY_COUNT;category++){
        native_front_state.selected=category;
        native_front_category_action(category,1);
        assert(native_front_category==category && native_front_state.selected==0);
        assert(native_front_options_blocked());test_now+=1000000;
        int count=native_front_option_count();
        for(int row=0;row<count;row++){
            native_front_category_action(row,-1);
            assert(changed_row==native_menu_category_option(category,row) && changed_direction==-1);
        }
        native_front_options_back();
        assert(native_front_state.open && native_front_category==-1 && native_front_state.selected==category);
        assert(native_front_options_blocked());test_now+=1000000;
    }
    native_front_options_back();assert(!native_front_state.open);
    puts("PASS frontend category entry/edit/back: correct shared setting IDs, parent focus and transition lock");
    puts("PASS original mode menu: Options/Exit follow Mission/Multiplayer, optional Cheats retained, matching hit regions, Previous-tab priority, no entries on other folders");
}
