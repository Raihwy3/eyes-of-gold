/* Read-only collection of original watch text layout. No display-list,
 * navigation, pause, watch animation or original text-global updates. */
extern s32 text_spacing;
static Gfx *native_objective_blackbox(Gfx *gdl,s32 *x,s32 *y,s32 *right,s32 *bottom) {
    return native_objective_is_collecting()?gdl:draw_blackbox_to_screen(gdl,x,y,right,bottom);
}
static void native_objective_overlap(s32 value) {
    if(native_objective_is_collecting())native_objective_overlap_value=value;
    else setTextOverlapCorrection(value);
}
static Gfx *native_objective_text(Gfx *gdl,s32 *x,s32 *y,char *text,struct fontchar *chars,struct font *font,u32 colour,s32 width,s32 height,u32 offset,s32 lineheight) {
    if(!native_objective_is_collecting())return textRender(gdl,x,y,text,chars,font,colour,width,height,offset,lineheight);
    if(!text || !native_objective_layout)return gdl;
    if(!lineheight)lineheight=chars['['].height+chars['['].baseline;
    if(j_text_trigger && lineheight<14)lineheight=14;
    if(native_objective_layout->count<EOG_OBJECTIVE_TEXTS) {
        struct EogObjectiveText *t=&native_objective_layout->text[native_objective_layout->count++];
        t->x=*x;t->y=*y+offset;t->width=width;t->height=height;t->lineheight=lineheight;t->overlap=native_objective_overlap_value;t->spacing=text_spacing;t->colour=colour;
        unsigned len=strlen(text);if(len>=sizeof t->text)len=sizeof t->text-1;memcpy(t->text,text,len);t->text[len]=0;
    }
    /* Match textRender's cursor updates: the original objective layout uses
     * its returned Y and its blank-line overlap for subsequent labels. */
    int savedx=*x,previous='H'-33;
    for(const unsigned char *p=(unsigned char *)text;*p;p++) {
        if(*p==' '){*x+=5;previous='H'-33;}
        else if(*p=='\n'){*y+=native_objective_overlap_value>=0 && savedx==*x?native_objective_overlap_value:lineheight;*x=savedx;previous='H'-33;}
        else if(*p>=33 && *p<=126){int c=*p-33;*x+=chars[c].width+1-font->kerning[chars[previous].kerningindex*13+chars[c].kerningindex]-text_spacing;previous=c;}
    }
    return gdl;
}
static void native_capture_objectives(struct EogObjectiveLayout *out) {
    if(!ptrFontBankGothic || !ptrFontBankGothicChars)return;
    native_objective_layout=out;native_objective_collecting=1;native_objective_overlap_value=-1;
    draw_watch_mission_briefing_page(NULL,NULL);
    native_objective_collecting=0;native_objective_layout=NULL;
}
