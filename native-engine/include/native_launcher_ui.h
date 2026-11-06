#ifndef EOG_NATIVE_LAUNCHER_UI_H
#define EOG_NATIVE_LAUNCHER_UI_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "native_version.h"
/* Console-only chrome shared by boot and update, before game assets exist.
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
}
static inline void native_launcher_top(const char *subtitle,const char *stage) {
    printf("\x1b[33m"
           "\x1b[3;5H+----------------------------------------+"
           "\x1b[4;5H|             FIELD OPERATIONS           |"
           "\x1b[5;5H+----------------------------------------+"
           "\x1b[8;23H  |  \x1b[9;23H--O--\x1b[10;23H  |  "
           "\x1b[12;14HE Y E S   O F   G O L D\x1b[0m");
    native_launcher_text(14,16,subtitle,32);
    native_launcher_text(18,9,stage,34);
}
static inline void native_launcher_bottom(const char *title) {
    printf("\x1b[33m\x1b[2;3H+----------------------------------+");
    for(int row=3;row<28;row++)printf("\x1b[%d;3H|\x1b[%d;38H|",row,row);
    printf("\x1b[28;3H+----------------------------------+\x1b[0m");
    native_launcher_text(3,5,"FIELD OPERATIONS",32);
    native_launcher_line(5,5,32);
    printf("\x1b[33m");native_launcher_text(6,5,title,32);printf("\x1b[0m");
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
    native_launcher_line(19,5,32);native_launcher_line(23,5,32);
    printf("\x1b[33m\x1b[20;21H|\x1b[21;21H|\x1b[22;21H|\x1b[0m");
    if(!strncmp(action,"A:",2))native_launcher_text(21,5,action,back && back-action<15?(int)(back-action):15);
    else native_launcher_text(21,5,"PLEASE WAIT",15);
    if(back)native_launcher_text(21,23,back,14);
}
#endif
