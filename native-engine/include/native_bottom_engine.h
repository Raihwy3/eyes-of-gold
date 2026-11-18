/* Original font glyphs are copied while owned by the game thread. */
#include "native_bottom.h"
static void native_bottom_copy_fonts(void) {
    static unsigned copied;
    if(!native_bottom_glyph)return;
    for(int font=0;font<2;font++) {
        struct fontchar *chars=font?ptrFontBankGothicChars:ptrFontZurichBoldChars;
        if(!chars || (copied&(1u<<font)))continue;
        for(int i=0;i<94;i++)native_bottom_glyph(font,i,chars[i].width,chars[i].height,chars[i].baseline,chars[i].pixeldata,(chars[i].width+7)&0xf8);
        if(native_bottom_spacing) {
            struct font *metrics=font?ptrFontBankGothic:ptrFontZurichBold;
            int classes[94];for(int i=0;i<94;i++)classes[i]=chars[i].kerningindex;
            if(metrics)native_bottom_spacing(font,metrics->kerning,classes);
        }
        copied|=1u<<font;
    }
}
static void native_bottom_string(char *out,unsigned capacity,const char *in) {
    unsigned n=0;
    if(in)while(*in && n+1<capacity) {unsigned char c=*in++;if(c=='\n' || c=='\r')c=' ';out[n++]=c>=32 && c<127?c:' ';}
    while(n && out[n-1]==' ')n--;
    out[n]=0;
}
static void native_bottom_row(struct EogBottomView *v,const char *label,const char *value,int tag,int disabled) {
    if(v->count>=EOG_BOTTOM_ROWS)return;
    struct EogBottomRow *r=&v->rows[v->count++];
    native_bottom_string(r->label,sizeof r->label,label);native_bottom_string(r->value,sizeof r->value,value);
    r->tag=tag;r->disabled=disabled;
}
static void native_bottom_paragraph(struct EogBottomView *v,const char *s) {
    while(s && *s && v->count<EOG_BOTTOM_ROWS) {
        while(*s==' ' || *s=='\n' || *s=='\r')s++;
        if(!*s)break;
        char line[64];unsigned n=0,last_space=0;
        while(s[n] && s[n]!='\n' && n<43){if(s[n]==' ')last_space=n;n++;}
        if(n==43 && s[n] && last_space)n=last_space;
        memcpy(line,s,n);line[n]=0;s+=n;
        native_bottom_row(v,line,"",-1,0);
    }
}
