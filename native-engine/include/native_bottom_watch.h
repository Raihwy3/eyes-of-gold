#include "native_bottom_engine.h"
#include "native_input.h"
#include "game/bondinv.h"
extern u8 *get_text_for_objective(int);
extern s32 get_difficulty_for_objective(s32);
extern u8 *get_ptr_text_for_watch_breifing_page(WATCH_BRIEFING_PAGE);
static struct EogBottomView native_bottom_watch_view;
static int native_bottom_abort,native_bottom_brief=-1;
static void native_bottom_objectives(struct EogBottomView *v) {
    for(int i=0;i<objectiveGetCount() && i<OBJECTIVES_MAX;i++)if(get_difficulty_for_objective(i)<=lvlGetSelectedDifficulty()) {
        int state=get_status_of_objective(i);
        native_bottom_row(v,(char *)get_text_for_objective(i),state==OBJECTIVESTATUS_COMPLETE?"Complete":state==OBJECTIVESTATUS_FAILED?"Failed":"Incomplete",i,0);
        v->rows[v->count-1].status=state==OBJECTIVESTATUS_FAILED?2:0;
    }
    if(!v->count)native_bottom_string(v->note,sizeof v->note,"No objectives for this mission.");
}
void native_bottom_game_tick(void) {
    if(get_cur_playernum()!=0)return;
    if(!native_bottom_publish || !g_CurrentPlayer || g_CurrentPlayer->watch_animation_state!=WATCH_ANIMATION_0x0)return;
    static struct EogBottomView v;memset(&v,0,sizeof v);
    native_bottom_abort=0;native_watch_brief_scroll=0;native_watch_options_reset();
    v.kind=EOG_BOTTOM_GAME;v.context=300;v.theme=native_setting_watch();
    if(getPlayerCount()>1) {
        native_bottom_string(v.title,sizeof v.title,"LOCAL MULTIPLAYER");
        native_bottom_string(v.note,sizeof v.note,"Player 1 controlled. Player 2 idle.");
        native_bottom_copy_fonts();native_bottom_publish(&v);return;
    }
    native_bottom_string(v.title,sizeof v.title,"MISSION OBJECTIVES");native_bottom_copy_fonts();
    native_bottom_objectives(&v);native_capture_objectives(&v.objectives);native_bottom_publish(&v);
}
/* Hit test the same 320x240 authored canvas used by the original page. */
static void native_bottom_watch_point(int x,int y) {
    if(y<(watch_screen_index==WATCH_INDEX_MISSION_STATUS?58:watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS?18:35)){
        if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS && native_watch_option_back())return;
        if(watch_item_is_actively_selected){watch_item_is_actively_selected=0;D_800409A4=0;}
        else if(native_input_press)native_input_press(START_BUTTON);
        return;
    }
    if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS) {
        int row=-1;
        for(int i=0;i<native_watch_option_count();i++) {
            int top=native_watch_option_y(i)-5;
            int bottom=i+1<native_watch_option_count()?native_watch_option_y(i+1)-5:native_watch_option_y(i)+12;
            if(y>=top && y<bottom){row=i;break;}
        }
        if(row<0)return;
        native_watch_nav.selected=row;
        native_watch_option_action(native_watch_option_tag(row),x<160?-1:1);
    } else if(watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS) {
        if(y>=20 && y<40){controller_options_index=CONTROLLER_OPTIONS_INDEX_STYLE;native_menu_option_change(0,x<160?-1:1);watch_item_is_actively_selected=0;}
        else if(y>=40 && y<58){controller_options_index=CONTROLLER_OPTIONS_INDEX_INPUTS;watch_item_is_actively_selected=!watch_item_is_actively_selected;}
    } else if(watch_screen_index==WATCH_INDEX_INVENTORY) {
        int pitch=j_text_trigger?14:12;
        if(y>=140 && y<140+5*pitch) {
            int index=(y-140-watch_inventory_text_y)/pitch;
            if(index>=0 && index<bondinvCountTotalItemsInInv()) {
                g_curWatchItemIndex=index;watch_inventory_cursor_pos=(float)index;
                watch_inventory_text_target_y=2*pitch-index*pitch;
                watch_item_is_actively_selected=0;if(native_input_press)native_input_press(A_BUTTON);
            }
        }
    } else if(watch_screen_index==WATCH_INDEX_MISSION_STATUS) {
        if(y>=70 && y<=94) {
            if(!watch_item_is_actively_selected){watch_item_is_actively_selected=1;D_800409A4=0;}
            else if(x>=175){D_800409A4=1;if(native_input_press)native_input_press(A_BUTTON);}
            else {watch_item_is_actively_selected=0;D_800409A4=0;}
        }
    } else if(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING) {
        native_watch_brief_scroll=0;
        mission_brief_index=(mission_brief_index+(x<160?4:1))%5;
        native_bottom_brief=mission_brief_index;
    }
}
static void native_bottom_watch_events(void) {
    if(!native_bottom_take)return;
    struct EogBottomCommand c;
    int context=native_watch_options_context();
    while(native_bottom_take(context,&c)) {
        if(c.kind==EOG_BOTTOM_ADJUST){
            if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS || watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS)
                native_bottom_watch_point(c.direction<0?80:220,c.index);
            if(native_watch_options_context()!=context)return;
            continue;
        }
        if(c.kind==EOG_BOTTOM_POINT){native_bottom_watch_point(c.index,c.direction);if(native_watch_options_context()!=context)return;continue;}
        if(c.kind==EOG_BOTTOM_SCROLL) {
            if(watch_screen_index==WATCH_INDEX_INVENTORY) {
                int next=g_curWatchItemIndex+c.direction,count=bondinvCountTotalItemsInInv();
                if(next>=0 && next<count){g_curWatchItemIndex=next;watch_inventory_cursor_pos=(float)next;watch_inventory_text_target_y=2*(j_text_trigger?14:12)-next*(j_text_trigger?14:12);}
            } else if(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING) {
                if(mission_brief_index!=BRIEF_INDEX_OBJECTIVES){native_watch_brief_scroll+=c.direction*12;if(native_watch_brief_scroll<0)native_watch_brief_scroll=0;if(native_watch_brief_scroll>4096)native_watch_brief_scroll=4096;}
            } else if(watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS)native_menu_option_change(0,c.direction);
            else if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS){native_watch_editing=0;watch_item_is_actively_selected=0;native_watch_nav.selected=(native_watch_nav.selected+c.direction+native_watch_option_count())%native_watch_option_count();game_options_index=native_watch_advanced?9:native_watch_nav.selected;}
            continue;
        }
        if(c.kind==EOG_BOTTOM_BACK && watch_screen_index==WATCH_INDEX_GAME_OPTIONS && native_watch_option_back())return;
        if(c.kind==EOG_BOTTOM_BACK && c.direction) {
            if(watch_item_is_actively_selected){watch_item_is_actively_selected=0;D_800409A4=0;}
            else if(native_input_press)native_input_press(START_BUTTON);
            return;
        }
        if(c.kind==EOG_BOTTOM_BACK) {
            if(watch_screen_index==WATCH_INDEX_MISSION_STATUS && watch_item_is_actively_selected){watch_item_is_actively_selected=0;D_800409A4=0;}
            else if(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING && native_bottom_brief>=0)native_bottom_brief=-1;
            else if(native_input_press)native_input_press(START_BUTTON);
            return;
        }
        if(c.kind==EOG_BOTTOM_PAGE && c.index>=0 && c.index<=4) {
            native_watch_brief_scroll=0;native_watch_options_reset();watch_screen_index=c.index;watch_item_is_actively_selected=0;native_bottom_abort=0;
            trigger_watch_zoom(c.index==0?WATCHZOOM2:c.index==2 || c.index==3?WATCHZOOM3:WATCHZOOM1,15.0f);
            set_controlstick_lr_disabled();return;
        }
        if(native_watch_options_context()!=context)return;
        if(native_bottom_watch_view.context!=context || c.index<0 || c.index>=native_bottom_watch_view.count)continue;
        int tag=native_bottom_watch_view.rows[c.index].tag;
        if(tag<0)continue;
        if(c.kind==EOG_BOTTOM_SELECT) {
            if(watch_screen_index==WATCH_INDEX_INVENTORY){g_curWatchItemIndex=tag;watch_inventory_cursor_pos=(float)tag;}
            else if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS)for(int i=0;i<native_watch_option_count();i++)if(native_watch_option_tag(i)==tag){native_watch_editing=0;watch_item_is_actively_selected=0;native_watch_nav.selected=i;game_options_index=native_watch_advanced?9:i;}
            continue;
        }
        if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS || watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS) {
            if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS){native_watch_editing=0;native_watch_option_action(tag,c.direction?c.direction:1);}
            else native_menu_option_change(tag,c.direction?c.direction:1);
            if(tag>0 && tag<NATIVE_WATCH_ADVANCED)game_options_index=tag-1;
            watch_item_is_actively_selected=0;
        } else if(watch_screen_index==WATCH_INDEX_INVENTORY) {
            if(tag>=0 && tag<bondinvCountTotalItemsInInv()) {
                g_curWatchItemIndex=tag;watch_inventory_cursor_pos=(float)tag;
                watch_inventory_text_target_y=-tag*10;watch_item_is_actively_selected=0;
                if(native_input_press)native_input_press(A_BUTTON);
            }
        } else if(watch_screen_index==WATCH_INDEX_MISSION_STATUS) {
            if(tag==1000)native_bottom_abort=1;
            else if(tag==1002)native_bottom_abort=0;
            else if(tag==1001) {
                watch_item_is_actively_selected=1;D_800409A4=1;
                if(native_input_press)native_input_press(A_BUTTON);
            } else {watch_screen_index=WATCH_INDEX_MISSION_BRIEFING;mission_brief_index=BRIEF_INDEX_OBJECTIVES;native_bottom_brief=4;}
        } else if(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING) {
            if(tag==2999)native_bottom_brief=-1;
            else if(tag>=2000 && tag<=2004){native_watch_brief_scroll=0;native_bottom_brief=tag-2000;mission_brief_index=tag-2000;}
        }
    }
}
static void native_bottom_watch_publish(void) {
    if(get_cur_playernum()!=0)return;
    if(!native_bottom_publish)return;
    if(get_currentmenu()!=MENU_RUN_STAGE)return;
    native_bottom_watch_events();native_bottom_copy_fonts();
    static struct EogBottomView v;memset(&v,0,sizeof v);
    v.context=native_watch_options_context();v.kind=EOG_BOTTOM_WATCH;v.page=watch_screen_index;v.theme=native_setting_watch();
    v.health=(int)(g_CurrentPlayer->bondhealth*100);v.armour=(int)(g_CurrentPlayer->bondarmour*100);
    const char *titles[]={"MISSION STATUS","INVENTORY","CONTROL OPTIONS","GAME OPTIONS","MISSION BRIEFING"};
    native_bottom_string(v.title,sizeof v.title,titles[watch_screen_index<5?watch_screen_index:0]);
    if(watch_screen_index==WATCH_INDEX_MISSION_STATUS) {
        if(native_bottom_abort){native_bottom_row(&v,"Confirm abort mission","",1001,0);native_bottom_row(&v,"Cancel","",1002,0);}
        else {native_bottom_objectives(&v);native_bottom_row(&v,"Abort mission","",1000,0);}
    } else if(watch_screen_index==WATCH_INDEX_INVENTORY) {
        for(int i=0;i<bondinvCountTotalItemsInInv() && i<EOG_BOTTOM_ROWS;i++) {
            native_bottom_row(&v,(char *)bondinvGetFirstTitlebyIndex(i),(char *)bondinvGetSecondTitlebyIndex(i),i,0);
            if(i==g_curWatchItemIndex)v.selected=i;
        }
    } else if(watch_screen_index==WATCH_INDEX_GAME_OPTIONS) {
        native_bottom_string(v.title,sizeof v.title,native_watch_options_title());
        for(int i=0;i<native_watch_option_count();i++) {
            int tag=native_watch_option_tag(i);char value[32];
            const char *label=tag==NATIVE_WATCH_MOTION?"Controls":tag==NATIVE_WATCH_AUDIO?"Audio":tag==NATIVE_WATCH_ADVANCED?"Advanced":tag==NATIVE_WATCH_EXPERIMENTAL?"Experimental":tag==NATIVE_WATCH_BACK?"Back":native_menu_option_label(tag);
            native_bottom_row(&v,label,tag<NATIVE_WATCH_ADVANCED?native_menu_option_text(tag,value,sizeof value):"",tag,0);
        }
        v.selected=native_watch_nav.selected;
    } else if(watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS) {
        int start=watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS?0:1,end=watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS?1:NATIVE_MENU_OPTIONS_COUNT;
        for(int i=start;i<end;i++){char b[32];native_bottom_row(&v,native_menu_option_label(i),native_menu_option_text(i,b,sizeof b),i,0);if(i==game_options_index+1)v.selected=v.count-1;}
        if(watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS) {
            const char *const vesper[]={"Circle Pad: move","C-stick: look","R/ZR: fire   L: aim","A: action   B: weapon","Y: crouch   X: reload","START: close watch"};
            const char *const jinx[]={"Circle Pad: move","X/B/Y/A: look up/down/left/right","D-right: action   D-down: weapon","D-up: reload   D-left: crouch","R/ZR: fire   L: aim","START: close watch"};
            const char *const *help=native_setting_style()==9?jinx:vesper;
            if(native_setting_style()>=8)for(int i=0;i<6;i++)native_bottom_row(&v,help[i],"",-1,0);
        }
    } else if(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING) {
        const char *sections[]={"Background","M briefing","Q branch","Moneypenny","Objectives"};
        if(native_bottom_brief<0){for(int i=0;i<5;i++)native_bottom_row(&v,sections[i],"",2000+i,0);}
        else {
            native_bottom_row(&v,"Back to sections","",2999,0);
            if(native_bottom_brief==4)native_bottom_objectives(&v);
            else {
                const char *s=(char *)get_ptr_text_for_watch_breifing_page((WATCH_BRIEFING_PAGE)(native_bottom_brief+1));
                native_bottom_paragraph(&v,s);
            }
        }
    }
    native_bottom_watch_view=v;native_bottom_publish(&v);
}
