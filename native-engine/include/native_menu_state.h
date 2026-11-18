#ifndef EOG_NATIVE_MENU_STATE_H
#define EOG_NATIVE_MENU_STATE_H
#include "native_pad.h"
#include "native_menu_options.h"
struct NativeMenuState {unsigned previous;int open,selected,direction,ticks;};
enum {NATIVE_MENU_PASS,NATIVE_MENU_CONSUME,NATIVE_MENU_CHANGE_LEFT,NATIVE_MENU_CHANGE_RIGHT,NATIVE_MENU_EXIT};
static inline int native_menu_step(struct NativeMenuState *s,unsigned held,int x,int y,int available,int options_hit,int exit_hit) {
    unsigned down=held&~s->previous;s->previous=held;
    if(!available){s->open=s->direction=s->ticks=0;return NATIVE_MENU_PASS;}
    if(down&EOG_SELECT)return NATIVE_MENU_EXIT;
    if(!s->open) {
        if((down&EOG_A) && exit_hit)return NATIVE_MENU_EXIT;
        if((down&EOG_Y) || ((down&EOG_A) && options_hit)) {
            s->open=1;s->direction=s->ticks=0;return NATIVE_MENU_CONSUME;
        }
        return NATIVE_MENU_PASS;
    }
    if(down&(EOG_B|EOG_Y)){s->open=0;return NATIVE_MENU_CONSUME;}
    int dir=(held&EOG_DUP)||y>40?-1:(held&EOG_DDOWN)||y< -40?1:
            (held&EOG_DLEFT)||x< -40?-2:(held&EOG_DRIGHT)||x>40?2:0;
    int step=0;
    if(dir!=s->direction){s->direction=dir;s->ticks=0;step=dir;}
    else if(dir && ++s->ticks>15 && s->ticks%6==0)step=dir;
    if(step==-1 || step==1)s->selected=(s->selected+step+NATIVE_MENU_OPTIONS_COUNT)%NATIVE_MENU_OPTIONS_COUNT;
    if(step==-2)return NATIVE_MENU_CHANGE_LEFT;
    if(step==2 || (down&EOG_A))return NATIVE_MENU_CHANGE_RIGHT;
    return NATIVE_MENU_CONSUME;
}
#endif
