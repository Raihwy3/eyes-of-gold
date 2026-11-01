int native_watch_mirror_page(void){return watch_screen_index;}
/* Included after the original options.c definitions. Uses its fonts, scrolling,
 * navigation and highlight colours; no desktop overlay is introduced. */
static int native_style_at(int ordinal,int count) {
    return count==6 && ordinal>=4?ordinal+4:ordinal;
}
static Gfx *native_draw_control_styles(Gfx *gdl) {
    static int last=-999,selected,previous,spacing,dirty;
    static float scroll;
    const int count=joyGetControllerCount()<2?6:10;
    int raw=g_CurrentPlayer->cur_player_control_type_0;
    if(raw!=last) {
        selected=count==6?(raw>=8?raw-4:raw<4?raw:0):raw;
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
    for(int i=0;i<count;++i)strcat(text,(char *)langGet(game_control_styles[10*native_style_at(i,count)]));
    int x=0xaa,y=0x1a,h=0,w=0;
    textMeasure(&h,&w,(u8 *)text,ptrFontBankGothicChars,ptrFontBankGothic,j_text_trigger?14:10);
    gdl=textRender(gdl,&x,&y,(u8 *)text,ptrFontBankGothicChars,ptrFontBankGothic,0x00aa00b0,w,j_text_trigger?14:10,spacing,j_text_trigger?14:10);
    u8 *name=langGet(game_control_styles[10*native_style_at(selected,count)]);
    textMeasure(&h,&w,name,ptrFontBankGothicChars,ptrFontBankGothic,j_text_trigger?14:10);
    x=0xaa;y=0x1a;
    return textRender(gdl,&x,&y,name,ptrFontBankGothicChars,ptrFontBankGothic,0xa0ffa0f0,w,100,0,j_text_trigger?14:10);
}
static Gfx *native_draw_controls_help(Gfx *gdl) {
    static const char *const vesper[]={"Circle Pad: move\n","C-stick: look\n","R/ZR: fire    L: aim\n","A: action / accept\n","B: weapon / back\n","Y: crouch    X: reload\n","START: watch\n"};
    static const char *const jinx[]={"Circle Pad: move\n","X/B/Y/A: look up/down/left/right\n","D-right: action   D-down: weapon\n","D-up: reload     D-left: crouch\n","R/ZR: fire    L: aim\n","Menus: A accept / B back\n","START: watch\n"};
    const char *const *lines=g_CurrentPlayer->cur_player_control_type_0==9?jinx:vesper;
    gdl=microcode_constructor(gdl);
    for(int i=0;i<7;++i)gdl=draw_options_labels(gdl,50,68+i*15,(char *)lines[i],0x00ff00b0,0,-1,0,0,0x3000b0,0);
    return gdl;
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
