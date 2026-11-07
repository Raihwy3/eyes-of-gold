/* Touch/frontend bridge, run only on the original game thread. */
#include "native_bottom_engine.h"
#include "native_result_ui.h"
static struct EogBottomView native_bottom_front_view;
static int native_bottom_front_focus,last_bottom_menu=-1;
static int native_bottom_front_context(void){return current_menu+(native_front_is_open()?100+(native_front_category+1)*100:0);}
static void native_bottom_front_position(int tag) {
    if(current_menu==MENU_FILE_SELECT) {
        if(folder_selected_for_deletion>=0)folder_selected_for_deletion_choice=tag==101;
        else if(tag<4){cursor_h_pos=native_bottom_folder_x[tag];cursor_v_pos=native_bottom_folder_y[tag];}
        else {struct rectbbox *r=tag==4?&folder_option_COPY_bound:&folder_option_ERASE_bound;cursor_h_pos=(r->left+r->right)/2;cursor_v_pos=(r->up+r->down)/2;}
    } else if(current_menu==MENU_MODE_SELECT){cursor_h_pos=170;cursor_v_pos=native_mode_y(tag)+6;}
    else if(current_menu==MENU_MP_OPTIONS) {
        highlight_players=highlight_scenario=highlight_gameselect=highlight_gamelength=0;
        highlight_character=highlight_weaponselect=highlight_health=highlight_controlstyle=highlight_aimadjustment=0;
        tab_prev_highlight=tab_start_highlight=0;
        if(tag==100){cursor_h_pos=TABS_LEFT_EDGE+5;cursor_v_pos=STARTTAB_TAB_BOTTOM-5;tab_start_highlight=1;}
        else if(tag==101){cursor_h_pos=TABS_LEFT_EDGE+5;cursor_v_pos=PREVTAB_TAB_TOP+10;tab_prev_highlight=1;}
        else {
            cursor_h_pos=170;cursor_v_pos=0x79+20*tag+5;
            if(tag==1)highlight_scenario=1;else if(tag==2)highlight_gameselect=1;
            else if(tag==3)highlight_gamelength=1;else if(tag==4)highlight_weaponselect=1;
            else if(tag==5)highlight_character=1;else if(tag==6)highlight_health=1;
            else if(tag==7)highlight_controlstyle=1;else if(tag==8)highlight_aimadjustment=1;
        }
    } else if(current_menu==MENU_MP_STAGE_SELECT) {
        if(tag==100){cursor_h_pos=TABS_LEFT_EDGE+5;cursor_v_pos=PREVTAB_TAB_TOP+10;tab_prev_highlight=1;}
        else {current_mp_stage_highlighted=tag;cursor_h_pos=80+85*(tag%4);cursor_v_pos=135+70*(tag/4);tab_prev_highlight=0;}
    }
    else if(current_menu==MENU_CHEAT) {
        if(tag==100){cursor_h_pos=TABS_LEFT_EDGE+5;cursor_v_pos=PREVTAB_TAB_TOP+10;tab_prev_highlight=1;}
        else if(tag>=0 && tag<totalunlockedcheats){int split=native_cheat_split(totalunlockedcheats);cursor_h_pos=tag<split?100:240;cursor_v_pos=native_cheat_row_y(tag<split?tag:tag-split,totalunlockedcheats)+5;cheathighlighted=tag;tab_prev_highlight=0;}
    }
    else if(current_menu==MENU_MISSION_SELECT)set_cursor_to_stage_solo(tag);
    else if(current_menu==MENU_DIFFICULTY)set_cursor_pos_difficulty(tag);
    else {cursor_h_pos=tag?TABS_LEFT_EDGE+5:NEXTTAB_CURSOR_HPOS;cursor_v_pos=tag?PREVTAB_TAB_TOP+10:NEXTTAB_CURSOR_VPOS;}
}
static void native_bottom_front_events(void) {
    if(!native_bottom_take)return;
    if(current_menu==MENU_RUN_STAGE)return;
    int context=native_bottom_front_context();
    struct EogBottomCommand c;
    while(native_bottom_take(context,&c)) {
        if(native_front_options_blocked())continue;
        if(c.kind==EOG_BOTTOM_BACK) {
            if(native_front_is_open())native_front_options_back();
            else if(native_input_press)native_input_press(B_BUTTON);
            return;
        }
        if(native_bottom_front_view.context!=context || c.index<0 || c.index>=native_bottom_front_view.count)continue;
        struct EogBottomRow *r=&native_bottom_front_view.rows[c.index];
        if(r->disabled || r->tag<0)continue;
        native_bottom_front_focus=c.index;
        if(native_front_is_open()) {
            native_front_state.selected=r->tag;
            if(c.kind!=EOG_BOTTOM_SELECT)native_front_category_action(r->tag,native_front_category<0?1:c.kind==EOG_BOTTOM_CHANGE?c.direction:1);
        } else {
            native_bottom_front_position(r->tag);
            if(c.kind==EOG_BOTTOM_ACTIVATE) {
                if(current_menu==MENU_MODE_SELECT && native_mode_extra()>=0)native_mode_activate();
                else if(native_input_press)native_input_press(A_BUTTON);
            }
        }
        if(native_bottom_front_context()!=context)return;
    }
}
static void native_bottom_front_publish(void) {
    if(!native_bottom_publish)return;
    if(current_menu==MENU_RUN_STAGE)return;
    static struct EogBottomView v;memset(&v,0,sizeof v);
    v.context=native_bottom_front_context();v.kind=EOG_BOTTOM_MENU;
    if(last_bottom_menu!=v.context){native_bottom_front_focus=0;last_bottom_menu=v.context;}
    native_bottom_copy_fonts();
    if(native_front_is_open()) {
        v.kind=EOG_BOTTOM_OPTIONS;v.selected=native_front_state.selected;
        native_bottom_string(v.title,sizeof v.title,native_menu_category_title(native_front_category));
        for(int i=0;i<native_front_option_count();i++){
            char b[32];int row=native_menu_category_option(native_front_category,i);
            native_bottom_row(&v,native_front_category<0?native_menu_category_title(i):native_menu_option_label(row),native_front_category<0?"":native_menu_option_text(row,b,sizeof b),i,0);
        }
    } else if(current_menu==MENU_FILE_SELECT) {
        native_bottom_string(v.title,sizeof v.title,folder_selected_for_deletion>=0?"DELETE FILE?":"SELECT FILE");
        if(folder_selected_for_deletion>=0){native_bottom_row(&v,"Delete file","",100,0);native_bottom_row(&v,"Cancel","",101,0);}
        else {for(int i=0;i<4;i++){char label[24];snprintf(label,sizeof label,"File %d",i+1);native_bottom_row(&v,label,"",i,0);}native_bottom_row(&v,"Copy","",4,0);native_bottom_row(&v,"Erase","",5,0);}
    } else if(current_menu==MENU_MODE_SELECT) {
        native_bottom_string(v.title,sizeof v.title,"SELECT MODE");
        native_bottom_row(&v,"Select Mission","",0,0);native_bottom_row(&v,"Multiplayer","",1,0);
        if(is_cheat_menu_available)native_bottom_row(&v,"Cheat Options","",2,0);
        native_bottom_row(&v,"OPTIONS","",is_cheat_menu_available?3:2,0);native_bottom_row(&v,"EXIT","",is_cheat_menu_available?4:3,0);
    } else if(current_menu==MENU_CHEAT) {
        native_bottom_string(v.title,sizeof v.title,"CHEAT OPTIONS");
        for(int i=0;i<totalunlockedcheats;i++) {
            int id=arrayUnlockedCheats[i];
            native_bottom_row(&v,(char *)native_cheat_label(id),(const char *)native_cheat_value(id,(u8 *)(native_cheat_active(id)?"On":"Off")),i,0);
        }
        native_bottom_row(&v,"Back","",100,0);
    } else if(current_menu==MENU_MP_OPTIONS) {
        native_bottom_string(v.title,sizeof v.title,"LOCAL MULTIPLAYER");
        native_bottom_string(v.note,sizeof v.note,"Player 1 controlled. Player 2 idle.");
        native_bottom_row(&v,"Start match","",100,0);
        native_bottom_row(&v,"Arena",(char *)langGet(multi_stage_setups[MP_stage_selected].folder_text_preset),2,!unlock_stage_select);
        native_bottom_row(&v,"Scenario",(char *)langGet(mp_player_counts[scenario].stage),1,0);
        native_bottom_row(&v,"Weapons",(char *)langGet(*(getPtrMPWeaponSetTextID())),4,!unlock_weapon_select);
        native_bottom_row(&v,"Match length",(char *)langGet(multi_game_lengths[game_length].text_preset),3,!unlock_game_length);
        native_bottom_row(&v,"Characters","",5,!unlock_chars);
        native_bottom_row(&v,"Handicap","",6,!unlock_handicap);
        native_bottom_row(&v,"Control style","",7,!unlock_control_style);
        native_bottom_row(&v,"Aim adjustment","",8,!unlock_aim_sight);
        native_bottom_row(&v,"Back","",101,0);
    } else if(current_menu==MENU_MP_STAGE_SELECT) {
        native_bottom_string(v.title,sizeof v.title,"SELECT ARENA");
        for(int i=MP_STAGE_RANDOM;i<MP_STAGE_SELECTED_MAX;i++)native_bottom_row(&v,(char *)langGet(multi_stage_setups[i].folder_text_preset),"",i,!check_if_mp_stage_unlocked(i));
        native_bottom_row(&v,"Back","",100,0);
    } else if(current_menu==MENU_MISSION_SELECT) {
        native_bottom_string(v.title,sizeof v.title,"SELECT MISSION");
        for(int i=0;mission_folder_setup_entries[i].folder_text_preset;i++)if(mission_folder_setup_entries[i].type==MISSION_PART) {
            struct mission_folder_setup *m=&mission_folder_setup_entries[i];
            int locked=get_highest_unlocked_difficulty_for_level(m->mission_num)<0;
            native_bottom_row(&v,locked?"Redacted":(char *)langGet(m->folder_text_preset),"",m->mission_num,locked);
        }
    } else if(current_menu==MENU_DIFFICULTY) {
        native_bottom_string(v.title,sizeof v.title,"SELECT DIFFICULTY");
        int highest=briefingpage>=0?get_highest_unlocked_difficulty_for_level(mission_folder_setup_entries[briefingpage].mission_num):0;
        for(int i=0;i<4;i++)native_bottom_row(&v,get_ptr_difficulty_name(i),"",i,i>highest);
    } else if(current_menu==MENU_BRIEFING || current_menu==MENU_MISSION_FAILED || current_menu==MENU_MISSION_COMPLETE) {
        const char *title=native_result_title(current_menu==MENU_BRIEFING,g_isBondKIA,mission_failed_or_aborted,current_menu!=MENU_BRIEFING && ptrbriefingdata && frontCompleteAllObjectivesAliveSuccess());
        native_bottom_string(v.title,sizeof v.title,title);
        native_bottom_row(&v,"Next","",0,0);native_bottom_row(&v,"Previous","",1,0);
    } else {v.kind=0;}
    if(v.kind!=EOG_BOTTOM_OPTIONS)v.selected=native_bottom_front_focus;
    if(current_menu==MENU_MODE_SELECT && !native_front_is_open())v.selected=native_mode_row();
    // Uppercase the bottom frontend only; game/watch text stays original.
    for(char *p=v.title;*p;p++)if(*p>='a' && *p<='z')*p-=32;
    for(char *p=v.note;*p;p++)if(*p>='a' && *p<='z')*p-=32;
    for(int i=0;i<v.count;i++) {
        for(char *p=v.rows[i].label;*p;p++)if(*p>='a' && *p<='z')*p-=32;
        for(char *p=v.rows[i].value;*p;p++)if(*p>='a' && *p<='z')*p-=32;
    }
    native_bottom_front_view=v;native_bottom_publish(&v);
}
