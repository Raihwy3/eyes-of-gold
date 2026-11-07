/* Included in the original front.c, using its backdrop, fonts and cursor. */
#include "native_menu_state.h"
#include "native_input.h"
#include "native_game.h"
static struct NativeMenuState native_front_state;
static int native_front_category=-1;
static uint64_t native_options_ready_at;
static int native_options_return_wait;
static void native_front_close_options(void) {
    native_front_state.open=0;native_options_return_wait=1;
    native_options_ready_at=sysGetMicroseconds()+1000000;
    native_front_state.previous=native_input_menu_buttons();
    native_front_state.direction=native_front_state.ticks=0;
}
static void native_front_open_options(void) {
    native_front_state.open=1;native_options_return_wait=0;
    native_front_category=-1;native_front_state.selected=0;
    native_front_state.previous=native_input_menu_buttons();
    native_front_state.direction=native_front_state.ticks=0;
    native_options_ready_at=sysGetMicroseconds()+1000000;
}
static int native_front_option_count(void){return native_menu_category_count(native_front_category);}
static void native_front_category_action(int ordinal,int direction) {
    if(ordinal<0 || ordinal>=native_front_option_count())return;
    if(native_front_category<0){
        if(direction<0)return;
        native_front_category=ordinal;native_front_state.selected=0;
        native_front_state.direction=native_front_state.ticks=0;
        native_options_ready_at=sysGetMicroseconds()+1000000;
        native_front_state.previous=native_input_menu_buttons();
    }else native_menu_option_change(native_menu_category_option(native_front_category,ordinal),direction);
}
static void native_front_options_back(void) {
    if(native_front_category<0){native_front_close_options();return;}
    native_front_state.selected=native_front_category;native_front_category=-1;
    native_front_state.open=1;native_front_state.direction=native_front_state.ticks=0;
    native_front_state.previous=native_input_menu_buttons();
    native_options_ready_at=sysGetMicroseconds()+1000000;
}
static int native_front_options_blocked(void) {
    return native_options_return_wait || (native_front_state.open && sysGetMicroseconds()<native_options_ready_at);
}
static int native_front_transition_input_blocked(unsigned held,int x,int y) {
    if(!native_front_options_blocked())return 0;
    if(native_options_return_wait && sysGetMicroseconds()>=native_options_ready_at &&
       !held && x>=-40 && x<=40 && y>=-40 && y<=40)native_options_return_wait=0;
    return 1; /* Consume the neutral release frame as well. */
}
static int native_front_available(void){return current_menu==MENU_MODE_SELECT;}
static int native_mode_y(int row){return 210+(is_cheat_menu_available?22:30)*row;}
static int native_mode_row(void) {
    int step=is_cheat_menu_available?22:30;
    int row=((int)cursor_v_pos-native_mode_y(0)+step/2-6)/step;
    int last=is_cheat_menu_available?4:3;
    return row<0?0:row>last?last:row;
}
static int native_mode_extra(void) {
    int row=native_mode_row()-(is_cheat_menu_available?3:2);
    return row>=0 && !frontCheckCursorOnPreviousTab()?row:-1;
}
static void native_mode_activate(void) {
    if(native_mode_extra()==0)native_front_open_options();
    else if(native_mode_extra()==1)native_game_request_quit();
}
static int native_front_is_open(void){return native_front_state.open && native_front_available();}
static int native_front_update(void) {
    int was_open=native_front_state.open;
    native_bottom_front_events();
    if(!was_open && native_front_state.open)was_open=1;
    static int previous_available;
    int available=native_front_available();
    if(available && !previous_available)native_menu_options_restore_audio();
    previous_available=available;
    if(native_front_transition_input_blocked(native_input_menu_buttons(),joyGetStickX(0),joyGetStickY(0))) {
        native_front_state.previous=native_input_menu_buttons();
        native_front_state.direction=native_front_state.ticks=0;
        g_MenuTimer=0;return 1;
    }
    int options_hit=available && native_mode_extra()==0;
    int exit_hit=available && native_mode_extra()==1;
    int previous_selected=native_front_state.selected;
    int action=native_menu_step(&native_front_state,native_input_menu_buttons(),joyGetStickX(0),joyGetStickY(0),available,options_hit,exit_hit);
    if(!was_open && native_front_state.open)native_front_open_options();
    if(was_open && !native_front_state.open)native_front_options_back();
    if(was_open && native_front_state.open && native_front_state.selected!=previous_selected &&
       (action==NATIVE_MENU_CONSUME) &&
       (native_front_state.direction==-1 || native_front_state.direction==1)){
        int direction=native_front_state.direction==-1?-1:1;
        native_front_state.selected=(previous_selected+direction+native_front_option_count())%native_front_option_count();
    }
    if(action==NATIVE_MENU_EXIT){native_game_request_quit();return 1;}
    if(action==NATIVE_MENU_CHANGE_LEFT || action==NATIVE_MENU_CHANGE_RIGHT)
        native_front_category_action(native_front_state.selected,action==NATIVE_MENU_CHANGE_LEFT?-1:1);
    if(action!=NATIVE_MENU_PASS)g_MenuTimer=0;
    return action!=NATIVE_MENU_PASS;
}
static Gfx *native_front_text(Gfx *gdl,int x,int y,const char *text,u32 colour) {
    return textRender(gdl,&x,&y,(u8 *)text,ptrFontZurichBoldChars,ptrFontZurichBold,colour,viGetX(),viGetY(),0,0);
}
static Gfx *native_front_draw(Gfx *gdl) {
    if(!native_front_available())return gdl;
    int width=viGetX(),height=viGetY();
    if(native_front_state.open) {
        struct FolderSelectColour top=unknown_folderselect_constructor,bottom=unknown_folderselect_constructor_0;
        gdl=clear_framebuffer_black(gdl);
        gDPSetScissor(gdl++,G_SC_NON_INTERLACE,0,0,width,height);
        gDPSetCombineLERP(gdl++,TEXEL0,PRIMITIVE,ENV_ALPHA,PRIMITIVE,0,0,0,PRIMITIVE,TEXEL0,PRIMITIVE,ENV_ALPHA,PRIMITIVE,0,0,0,PRIMITIVE);
        gDPSetEnvColor(gdl++,255,255,255,20);
        gdl=titleRenderFolderMenuBackground(gdl,0,&top,&bottom);
        gSPDisplayList(gdl++,&dlBasicGeometry);
        gSPDisplayList(gdl++,&dlFastPipelineSetup);
        gdl=microcode_constructor(gdl);setTextSpacingInverted(0);
        gdl=native_front_text(gdl,width/12,height/9,native_menu_category_title(native_front_category),0xebd879ff);
        int first=(native_front_state.selected/7)*7;
        char text[32];snprintf(text,sizeof text,"%d / %d\n",first/7+1,(native_front_option_count()+6)/7);
        gdl=native_front_text(gdl,width*4/5,height/9,text,0xebd879ff);
        for(int i=0;i<7 && first+i<native_front_option_count();i++) {
            int ordinal=first+i,row=native_menu_category_option(native_front_category,ordinal),y=height/4+i*(height/14);
            u32 colour=ordinal==native_front_state.selected?0xebd879ff:0xffffffff;
            gdl=native_front_text(gdl,width/12,y,(native_front_category<0?native_menu_category_title(ordinal):native_menu_option_label(row)),colour);
            gdl=native_front_text(gdl,width*7/12,y,(native_front_category<0?"":native_menu_option_text(row,text,sizeof text)),colour);
        }
        gdl=native_front_text(gdl,width/12,height*4/5,(native_front_category<0?"Up/Down: select   A: open\n":"Up/Down: select   Left/Right: change\n"),0xffffffff);
        gdl=native_front_text(gdl,width/12,height*9/10,"B: back    Select: exit game\n",0xebd879ff);
        return gdl;
    }
    return gdl;
}
static Gfx *native_mode_draw_entries(Gfx *gdl) {
    int first=is_cheat_menu_available?3:2;
    for(int entry=0;entry<2;entry++) {
        int row=first+entry,x=0x96,y=native_mode_y(row),h,w;
        char number[8];snprintf(number,sizeof number,"%d.\n",row+1);
        const char *label=entry?"EXIT\n":"OPTIONS\n";
        textMeasure(&h,&w,(u8 *)label,ptrFontZurichBoldChars,ptrFontZurichBold,0);
        if(native_mode_extra()==entry)
            gdl=microcode_constructor_related_to_menus(gdl,0x94,y-2,0xaf+w,y+14,0x32);
        gdl=frontPrintText(gdl,&x,&y,number,ptrFontZurichBoldChars,ptrFontZurichBold,0xff,viGetX(),viGetY(),0,0);
        x=0xaa;y=native_mode_y(row);
        gdl=frontPrintText(gdl,&x,&y,(u8 *)label,ptrFontZurichBoldChars,ptrFontZurichBold,0xff,viGetX(),viGetY(),0,0);
    }
    return gdl;
}
