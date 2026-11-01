/* Include the generated original frontend so the real cast timer/terminator
 * and transition state are exercised, without rendering or loading a demo. */
#include "front.c"
#include <stdlib.h>
#include <stdio.h>
s32 g_ClockTimer=1;
f32 portNativeAspect(void) {return 5.0f/3.0f;}
void sysLogPrintf(enum LogLevel level,const char *fmt,...) {(void)level;(void)fmt;}
void viSetFovY(f32 f) {(void)f;}
void viSetZRange(f32 a,f32 b) {(void)a;(void)b;}
void viSetUseZBuf(s32 n) {(void)n;}
void viSetAspect(f32 f) {(void)f;}
void set_cur_player_screen_size(u32 a,u32 b) {(void)a;(void)b;}
void viSetViewSize(s16 a,s16 b) {(void)a;(void)b;}
void set_cur_player_viewport_size(u32 a,u32 b) {(void)a;(void)b;}
void viSetViewPosition(s16 a,s16 b) {(void)a;(void)b;}
s32 check_aztec_completed_any_folder_secret_00(void) {return 1;}
s32 fileIsEgyptCompletedOn00AnyFolder(void) {return 1;}
u32 randomGetNext(void) {return 1;}
u16 joyGetButtonsPressedThisFrame(s8 p,u16 b) {(void)p;(void)b;return 0;}
void select_ramrom_to_play(void) {abort();}
int main(void) {
    unsigned end=0;while(intro_char_table[end].body>=0)++end;
    if(!end)abort();
    intro_character_index=end-1;full_actor_intro=0;g_MenuTimer=181;
    menu_update=MENU_INVALID;is_emulating_spectrum=FALSE;
    interface_menu18_displaycast();
    if(menu_update!=MENU_FILE_SELECT || is_emulating_spectrum || intro_character_index!=0)abort();
    puts("PASS real cast intro completion returns to file select without launching attract demo");
}
