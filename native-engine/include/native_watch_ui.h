int native_watch_mirror_page(void){return watch_screen_index;}
/* Included after the original options.c definitions. Uses its fonts, scrolling,
 * navigation and highlight colours; no desktop overlay is introduced. */
static int native_control_rebind_row;
static int native_style_at(int ordinal,int count) {
    return count==8 && ordinal>=4?ordinal+4:ordinal;
}
static Gfx *native_draw_control_styles(Gfx *gdl) {
    static int last=-999,selected,previous,spacing,dirty;
    static float scroll;
    const int count=joyGetControllerCount()<2?8:12;
    int raw=g_CurrentPlayer->cur_player_control_type_0;
    if(raw!=last) {
        selected=count==8?(raw>=8?raw-4:raw<4?raw:0):raw;
        if(selected<0 || selected>=count)selected=0;
        previous=selected;scroll=(float)selected;spacing=-(j_text_trigger?14:10)*selected;dirty=1;
        last=raw;
    }
    if(watch_item_is_actively_selected && controller_options_index==CONTROLLER_OPTIONS_INDEX_STYLE) {
        sub_GAME_7F0A611C(&scroll,&selected,count,&spacing,&previous,&dirty,0,1,j_text_trigger?14:10);
        int next=native_style_at(selected,count);
        if(next!=raw) {
            native_setting_set_style(next);cur_player_set_control_type(next);last=next;
        }
    }
    char text[512];text[0]=0;
    for(int i=0;i<count;++i){int s=native_style_at(i,count);strcat(text,s==10?"1.7 Custom\n":s==11?"1.8 Natalya\n":(char *)langGet(game_control_styles[10*s]));}
    int x=0xaa,y=0x1a,h=0,w=0;
    textMeasure(&h,&w,(u8 *)text,ptrFontBankGothicChars,ptrFontBankGothic,j_text_trigger?14:10);
    gdl=textRender(gdl,&x,&y,(u8 *)text,ptrFontBankGothicChars,ptrFontBankGothic,0x00aa00b0,w,j_text_trigger?14:10,spacing,j_text_trigger?14:10);
    int cur_s=native_style_at(selected,count);
    u8 *name=cur_s==10?(u8 *)"1.7 Custom\n":cur_s==11?(u8 *)"1.8 Natalya\n":langGet(game_control_styles[10*cur_s]);
    textMeasure(&h,&w,name,ptrFontBankGothicChars,ptrFontBankGothic,j_text_trigger?14:10);
    x=0xaa;y=0x1a;
    return textRender(gdl,&x,&y,name,ptrFontBankGothicChars,ptrFontBankGothic,0xa0ffa0f0,w,100,0,j_text_trigger?14:10);
}
static Gfx *native_draw_controls_help(Gfx *gdl) {
    static const char *const vesper[]={"Circle Pad: move\n","C-stick: look\n","R/ZR: fire    L: aim\n","A: action / accept\n","B: weapon / back\n","Y: crouch    X: reload\n","START: watch\n"};
    static const char *const jinx[]={"Circle Pad: move\n","X/B/Y/A: look up/down/left/right\n","D-right: action   D-down: weapon\n","D-up: reload     D-left: crouch\n","R/ZR: fire    L: aim\n","Menus: A accept / B back\n","START: watch\n"};
    static const char *const natalya[]={"Circle Pad: look\n","X/B/Y/A: move/strafe\n","D-right: action   D-down: weapon\n","D-up: reload     D-left: crouch\n","R/ZR: fire    L: aim\n","Menus: A accept / B back\n","START: watch\n"};
    gdl=microcode_constructor(gdl);
    if(g_CurrentPlayer->cur_player_control_type_0==10) {
        for(int i=0;i<9;++i){
            int row=54+i;char value[32];
            unsigned colour=(controller_options_index==CONTROLLER_OPTIONS_INDEX_INPUTS && i==native_control_rebind_row)?(watch_item_is_actively_selected?-1u:0xa0ffa0f0):0x00ff00b0;
            gdl=draw_options_labels(gdl,50,66+i*16,(char *)native_menu_option_label(row),colour,0,-1,0,0,0x3000b0,0);
            gdl=draw_options_labels(gdl,195,66+i*16,(char *)native_menu_option_text(row,value,sizeof value),colour,0,-1,0,0,0x3000b0,0);
        }
        return gdl;
    }
    const char *const *lines=g_CurrentPlayer->cur_player_control_type_0==11?natalya:g_CurrentPlayer->cur_player_control_type_0==9?jinx:vesper;
    for(int i=0;i<7;++i)gdl=draw_options_labels(gdl,50,68+i*15,(char *)lines[i],0x00ff00b0,0,-1,0,0,0x3000b0,0);
    return gdl;
}
static int native_control_rebind_input(void) {
    if(g_CurrentPlayer->cur_player_control_type_0!=10)return 0;
    unsigned pressed=joyGetButtonsPressedThisFrame(PLAYER_1,0xffff);
    if(pressed&A_BUTTON){watch_item_is_actively_selected=!watch_item_is_actively_selected;return 1;}
    if(watch_item_is_actively_selected){
        if(pressed&B_BUTTON){watch_item_is_actively_selected=0;return 1;}
        if(pressed&(L_JPAD|L_CBUTTONS))native_menu_option_change(54+native_control_rebind_row,-1);
        else if(pressed&(R_JPAD|R_CBUTTONS))native_menu_option_change(54+native_control_rebind_row,1);
        return 1;
    }
    if(pressed&(L_JPAD|L_CBUTTONS))native_menu_option_change(54+native_control_rebind_row,-1);
    else if(pressed&(R_JPAD|R_CBUTTONS))native_menu_option_change(54+native_control_rebind_row,1);
    else if(pressed&B_BUTTON)controller_options_index=CONTROLLER_OPTIONS_INDEX_STYLE;
    else if(pressed&(U_JPAD|U_CBUTTONS)){
        if(native_control_rebind_row>0)native_control_rebind_row--;
        else controller_options_index=CONTROLLER_OPTIONS_INDEX_STYLE;
    }else if(pressed&(D_JPAD|D_CBUTTONS)){
        if(native_control_rebind_row<8)native_control_rebind_row++;
        else controller_options_index=CONTROLLER_OPTIONS_INDEX_STYLE;
    }
    return 1;
}
static Gfx *native_draw_colour_choice(Gfx *gdl,int y,int index,unsigned state) {
    if(state==2)game_option_toggle_input(index);
    int value=index==8?native_setting_crosshair():index==9?native_setting_watch():index==10?native_setting_aa():native_setting_fps()==60;
    const char *label=index==11?(value?"60 FPS\n":"30 FPS\n"):index==10?(value?"On\n":"Off\n"):native_colour_name(index==9,value);
    return draw_options_labels(gdl,200,y,(char *)label,state==2?0xa0ffa0f0:state==1?0x00ff00b0:0x00800080,0,-1,0,0,0x3000b0,0);
}
static Gfx *native_watch_theme_scope(Gfx *gdl,int enabled) {
    gDma0p(gdl++,0x3a,enabled?NATIVE_WATCH_SCOPE:0,enabled?0xffffffu:(~NATIVE_WATCH_SCOPE)&0xffffffu);
    return gdl;
}
