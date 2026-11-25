#ifndef EOG_NATIVE_LAUNCHER_UI_H
#define EOG_NATIVE_LAUNCHER_UI_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "native_version.h"
#ifdef __3DS__
#include <3ds.h>
#include "native_launcher_art.h"
static PrintConsole *native_launcher_console;
static inline void native_launcher_begin(PrintConsole *console){native_launcher_console=console;}
static inline void native_launcher_pixel(int x,int y,uint16_t colour){
    if(native_launcher_console && x>=0 && x<native_launcher_console->consoleWidth*8 && y>=0 && y<240)
        native_launcher_console->frameBuffer[x*240+239-y]=colour;
}
static inline void native_launcher_rect(int x,int y,int w,int h,uint16_t colour){
    for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)native_launcher_pixel(xx,yy,colour);
}
static inline void native_launcher_decorate(PrintConsole *console,int top_screen){
    native_launcher_begin(console);
    for(int x=0;x<console->consoleWidth*8;x++)for(int y=0;y<240;y++){
        uint16_t *pixel=&console->frameBuffer[x*240+239-y];
        if(!*pixel)*pixel=0x0041; /* same ink-dark background as the artwork */
        else if(*pixel==0xaaa0 || *pixel==0xffe0)*pixel=0xe609;
    }
    const uint16_t *art=top_screen?native_launcher_logo:native_launcher_eye;
    int w=top_screen?200:32,h=top_screen?100:32,left=top_screen?100:24,top=top_screen?0:4;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++)native_launcher_pixel(left+x,top+y,art[y*w+x]);
}
#endif
/* Shared boot/update layout, available before game assets exist.
 * Coordinates use the SDK's 8px font: top 50x30, bottom 40x30. */
static inline void native_launcher_line(int row,int column,int width) {
    printf("\x1b[33m\x1b[%d;%dH",row,column);
    for(int i=0;i<width;i++)putchar('-');
    printf("\x1b[0m");
}
static inline void native_launcher_text(int row,int column,const char *s,int width) {
    printf("\x1b[%d;%dH",row,column);
    for(int i=0;s && s[i] && i<width;i++) {
        unsigned char c=(unsigned char)s[i];putchar(c>=32 && c<127?c:' ');
    }
}
static inline unsigned native_launcher_percent(unsigned done,unsigned total) {
    if(!total)return 0;
    uint64_t value=(uint64_t)done*100/total;return value>100?100:(unsigned)value;
}
static inline void native_launcher_bar(int row,int column,int width,unsigned done,unsigned total) {
    unsigned filled=native_launcher_percent(done,total)*(unsigned)width/100;
    printf("\x1b[33m\x1b[%d;%dH[",row,column);
    for(int i=0;i<width;i++)putchar((unsigned)i<filled?'=':'-');
    printf("]\x1b[0m");
#ifdef __3DS__
    int x=(column-1)*8,y=(row-1)*8;
    native_launcher_rect(x,y,(width+2)*8,8,0x0041);
    native_launcher_rect(x,y+2,(width+2)*8,4,0x39a5);
    native_launcher_rect(x+1,y+3,(int)((uint64_t)native_launcher_percent(done,total)*((width+2)*8-2)/100),2,0xe609);
#endif
}
static inline void native_launcher_top(const char *subtitle,const char *stage) {
    printf("\x1b[33m");native_launcher_text(12,18,"Eyes of Gold",26);printf("\x1b[0m");
    native_launcher_text(14,16,subtitle,32);
    native_launcher_text(18,9,stage,34);
}
static inline void native_launcher_bottom(const char *title) {
    printf("\x1b[33m");native_launcher_text(3,9,title,28);printf("\x1b[0m");
    native_launcher_line(6,5,32);
    native_launcher_line(24,5,32);
    native_launcher_text(26,5,"Eyes of Gold " NATIVE_ENGINE_VERSION,32);
}
/* Word-wrap within a panel, including unusually long SD/network errors.
 * Never allow message bytes to move the console cursor or overwrite chrome. */
static inline void native_launcher_message(int row,const char *s,int lines) {
    int column=0;printf("\x1b[%d;5H",row);
    while(s && *s && lines>0) {
        if(*s=='\n'){s++;column=0;row++;if(--lines)printf("\x1b[%d;5H",row);continue;}
        while(*s==' ' && !column)s++;
        if(!*s)break;
        int length=0;while(s[length] && s[length]!=' ' && s[length]!='\n')length++;
        if(column && length>32-column){column=0;row++;if(!--lines)break;printf("\x1b[%d;5H",row);}
        do {
            unsigned char c=(unsigned char)*s++;
            putchar(c>=32 && c<127?c:' ');
            if(++column==32){column=0;row++;if(!--lines)return;printf("\x1b[%d;5H",row);}
        } while(*s && *s!=' ' && *s!='\n');
        if(*s==' '){s++;if(column)putchar(' '),column++;if(column==32){column=0;row++;if(!--lines)return;printf("\x1b[%d;5H",row);}}
    }
}
static inline void native_launcher_actions(const char *action) {
    const char *back=strstr(action,"B:");
    if(!strncmp(action,"A:",2))native_launcher_text(21,5,action,back && back-action<15?(int)(back-action):15);
    else native_launcher_text(21,5,"PLEASE WAIT",15);
    if(back)native_launcher_text(21,23,back,14);
}
#endif
