/* Explicit pages drive original-font rendering and touch hit testing together.
 * Audio rows stay out of Advanced; new options must be assigned to a page. */
#include "native_menu_state.h"
#include "native_input.h"
#define NATIVE_WATCH_ADVANCED 10000
#define NATIVE_WATCH_BACK 10001
#define NATIVE_WATCH_AUDIO 10002
#define NATIVE_WATCH_EXPERIMENTAL 10003
#define NATIVE_WATCH_OPTION_Y 40
#define NATIVE_WATCH_OPTION_PITCH 10
static int native_watch_advanced,native_watch_audio,native_watch_experimental,native_watch_wait_neutral,native_watch_editing;
static struct NativeMenuState native_watch_nav;
static int native_watch_option_count(void){return native_watch_experimental?3:native_watch_audio?4:native_watch_advanced?17:9;}
static int native_watch_option_y(int index) {return NATIVE_WATCH_OPTION_Y+index*(native_watch_advanced?NATIVE_WATCH_OPTION_PITCH:16);}
static int native_watch_option_tag(int index) {
    if(native_watch_experimental){static const int rows[]={27,28,NATIVE_WATCH_BACK};return rows[index];}
    if(native_watch_audio){static const int rows[]={1,2,25,NATIVE_WATCH_BACK};return rows[index];}
    if(!native_watch_advanced)return index<7?index+3:index==7?NATIVE_WATCH_AUDIO:NATIVE_WATCH_ADVANCED;
    static const int rows[]={15,14,10,13,22,23,21,16,17,24,18,19,12,20,11,NATIVE_WATCH_EXPERIMENTAL,NATIVE_WATCH_BACK};return rows[index];
}
static const char *native_watch_options_title(void){return native_watch_experimental?"EXPERIMENTAL":native_watch_audio?"AUDIO":native_watch_advanced?"ADVANCED":"GAME OPTIONS";}
static int native_watch_options_context(void){return 200+watch_screen_index+(watch_screen_index==WATCH_INDEX_GAME_OPTIONS?(native_watch_experimental?600:native_watch_audio?400:native_watch_advanced?200:0):0);}
static void native_watch_options_reset(void) {
    native_watch_experimental=native_watch_audio=native_watch_advanced=native_watch_editing=0;if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS)watch_item_is_actively_selected=0;native_watch_wait_neutral=1;native_watch_nav=(struct NativeMenuState){0};
}
static void native_watch_option_action(int tag,int direction) {
    if(tag==NATIVE_WATCH_AUDIO){native_watch_audio=1;native_watch_nav.selected=0;}
    else if(tag==NATIVE_WATCH_ADVANCED){native_watch_advanced=1;native_watch_nav.selected=0;}
    else if(tag==NATIVE_WATCH_EXPERIMENTAL){native_watch_experimental=1;native_watch_nav.selected=0;}
    else if(tag==NATIVE_WATCH_BACK && native_watch_experimental){native_watch_experimental=0;native_watch_nav.selected=15;}
    else if(tag==NATIVE_WATCH_BACK){native_watch_nav.selected=native_watch_audio?7:8;native_watch_audio=native_watch_advanced=0;}
    else native_menu_option_change(tag,direction);
    native_watch_nav.previous=native_input_menu_buttons();
    if(tag>=NATIVE_WATCH_ADVANCED){native_watch_editing=0;native_watch_nav.direction=native_watch_nav.ticks=0;}
    game_options_index=native_watch_advanced?9:native_watch_nav.selected;
    watch_item_is_actively_selected=0;
}
static int native_watch_option_back(void) {
    if(native_watch_editing){native_watch_editing=0;watch_item_is_actively_selected=0;return 1;}
    if(!native_watch_advanced && !native_watch_audio)return 0;
    native_watch_option_action(NATIVE_WATCH_BACK,1);return 1;
}
static void native_watch_options_input(void) {
    watch_item_is_actively_selected=0;
    unsigned held=native_input_menu_buttons(),down=held&~native_watch_nav.previous;
    if(native_watch_wait_neutral) {
        native_watch_nav.previous=held;
        if(!(held&(EOG_A|EOG_B|EOG_DUP|EOG_DDOWN|EOG_DLEFT|EOG_DRIGHT|EOG_L|EOG_R)) &&
           joyGetStickX(0)>=-40 && joyGetStickX(0)<=40)native_watch_wait_neutral=0;
        return;
    }
    if(down&EOG_L || (!native_watch_editing && down&EOG_DLEFT)){watch_screen_index=WATCH_INDEX_CONTROL_OPTIONS;reset_controller_options_index();set_controlstick_lr_disabled();native_watch_options_reset();return;}
    if(down&EOG_R || (!native_watch_editing && down&EOG_DRIGHT)){watch_screen_index=WATCH_INDEX_MISSION_BRIEFING;trigger_watch_zoom(WATCHZOOM1,15.f);set_controlstick_lr_disabled();native_watch_options_reset();return;}
    if((down&EOG_B) && native_watch_editing){native_watch_editing=0;native_watch_nav.previous=held;return;}
    if(down&EOG_A){
        int tag=native_watch_option_tag(native_watch_nav.selected);
        native_watch_nav.previous=held;
        if(tag>=NATIVE_WATCH_ADVANCED)native_watch_option_action(tag,1);
        else native_watch_editing=!native_watch_editing;
        return;
    }
    int selected=native_watch_nav.selected;
    // Reuse the bounded stick/D-pad repeats, with this subpage's row count.
    native_watch_nav.open=1;
    int action=native_menu_step(&native_watch_nav,held&~EOG_SELECT,joyGetStickX(0),joyGetStickY(0),1,0,0);
    if(!native_watch_nav.open) {
        if(!native_watch_option_back() && native_input_press)native_input_press(START_BUTTON);
        native_watch_nav.open=1;return;
    }
    if(native_watch_nav.selected!=selected) {
        native_watch_editing=0;
        int step=(held&EOG_DUP)||joyGetStickY(0)>40?-1:1;
        native_watch_nav.selected=(selected+step+native_watch_option_count())%native_watch_option_count();
    }
    if(!native_watch_editing && (action==NATIVE_MENU_CHANGE_LEFT || action==NATIVE_MENU_CHANGE_RIGHT)) {
        watch_screen_index=action==NATIVE_MENU_CHANGE_LEFT?WATCH_INDEX_CONTROL_OPTIONS:WATCH_INDEX_MISSION_BRIEFING;
        if(action==NATIVE_MENU_CHANGE_LEFT)reset_controller_options_index();
        else trigger_watch_zoom(WATCHZOOM1,15.f);
        set_controlstick_lr_disabled();native_watch_options_reset();return;
    }
    if(native_watch_editing && (action==NATIVE_MENU_CHANGE_LEFT || action==NATIVE_MENU_CHANGE_RIGHT))
        native_watch_option_action(native_watch_option_tag(native_watch_nav.selected),action==NATIVE_MENU_CHANGE_LEFT?-1:1);
    game_options_index=native_watch_advanced?9:native_watch_nav.selected;
    watch_item_is_actively_selected=native_watch_editing;
}
static Gfx *native_watch_options_draw(Gfx *gdl,Mtx *matrix) {
    game_options_index=native_watch_advanced?9:native_watch_nav.selected;
    watch_item_is_actively_selected=native_watch_editing;
    gdl=draw_background_health_and_armor(gdl,matrix,0);
    if(check_watch_page_transistion_running()==1)return gdl;
    gdl=microcode_constructor(gdl);
    char heading[32];snprintf(heading,sizeof heading,"%s\n",native_watch_options_title());
    gdl=draw_options_labels(gdl,50,20,heading,0x00ff00b0,0,-1,0,0,0x3000b0,0);
    int count=native_watch_option_count();
    if(native_watch_nav.selected<0 || native_watch_nav.selected>=count)native_watch_nav.selected=0;
    for(int i=0;i<count;i++) {
        int tag=native_watch_option_tag(i),y=native_watch_option_y(i);
        unsigned colour=i==native_watch_nav.selected?0xa0ffa0f0:0x00ff00b0;
        const char *label=tag==NATIVE_WATCH_AUDIO?"Audio\n":tag==NATIVE_WATCH_ADVANCED?"Advanced\n":tag==NATIVE_WATCH_EXPERIMENTAL?"Experimental\n":tag==NATIVE_WATCH_BACK?"Back\n":native_menu_option_label(tag);
        gdl=draw_options_labels(gdl,50,y,(char *)label,colour,0,-1,0,0,0x3000b0,0);
        if(tag<NATIVE_WATCH_ADVANCED){char value[32];gdl=draw_options_labels(gdl,200,y,(char *)native_menu_option_text(tag,value,sizeof value),colour,0,-1,0,0,0x3000b0,0);}
    }
    return gdl;
}
