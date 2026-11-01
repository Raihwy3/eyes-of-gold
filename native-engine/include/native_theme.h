#ifndef EOG_NATIVE_THEME_H
#define EOG_NATIVE_THEME_H
#define NATIVE_WATCH_BODY_SCOPE 0x100000u
#define NATIVE_WATCH_TABS_SCOPE 0x200000u
#define NATIVE_WATCH_SCOPE 0x800000u
/* Recolour green UI chroma while retaining pale highlights, warning colours
 * and original alpha. This does not modify any texture or source vertex. */
static inline void native_theme_rgb(int theme,unsigned *r,unsigned *g,unsigned *b) {
    static const unsigned palette[6][3]={{0,255,0},{255,255,255},{255,176,32},{255,48,48},{64,160,255},{230,190,64}};
    if(theme<1 || theme>5 || *g<*r || *g<*b)return;
    unsigned neutral=*r<*b?*r:*b,green=*g-neutral;
    *r=neutral+(green*palette[theme][0]+127)/255;
    *g=neutral+(green*palette[theme][1]+127)/255;
    *b=neutral+(green*palette[theme][2]+127)/255;
}
#endif
