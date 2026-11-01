/* Exercise the actual options overlay and the shared frontend state machine. */
#define EOG_OBJECTIVE_CAPTURE_TEST 1
#include "options.c"
#define SPACE_WIDTH 5
#include "native-objective-measure.h"
#include "native_menu_state.h"
#include "native_test_assert.h"
#include <stdlib.h>
#include <unistd.h>
static int test_player_num,test_player_count=1;
s32 get_cur_playernum(void){return test_player_num;}
s32 getPlayerCount(void){return test_player_count;}
static struct player test_player;
struct player *g_CurrentPlayer=&test_player;
s32 j_text_trigger;
ALBank *g_musicSfxBufferPtr;
static u16 effects=32767,music=32767;
static struct EogBottomView captured;
static struct EogBottomCommand bottom_command;
static int command_pending;
static unsigned injected,watch_held;
static int stick_x,stick_y;
unsigned native_input_menu_buttons(void){return watch_held;}
s8 joyGetStickX(s8 pad){(void)pad;return stick_x;}
s8 joyGetStickY(s8 pad){(void)pad;return stick_y;}
s32 text_spacing;
u8 *langGet(s32 id){return (u8 *)(id==0xac28?"Complete\n":id==0xac29?"Incomplete\n":id==0xac37?"Failed\n":"OBJECTIVES\n");}
static int objective_state[3]={0,1,0};
MENU get_currentmenu(void){return MENU_RUN_STAGE;}
struct fontchar *ptrFontBankGothicChars,*ptrFontZurichBoldChars;
struct font *ptrFontBankGothic,*ptrFontZurichBold;
void native_bottom_publish(const struct EogBottomView *v){captured=*v;}
int native_bottom_take(int context,struct EogBottomCommand *c){if(!command_pending)return 0;command_pending=0;*c=bottom_command;return c->context==context;}
void native_input_press(unsigned b){injected|=b;}
int objectiveGetCount(void){return 3;}
s32 get_difficulty_for_objective(s32 i){return i;}
u8 *get_text_for_objective(int i){static char *names[]={"First objective","Second objective","Hard objective"};return (u8 *)names[i];}
OBJECTIVESTATUS get_status_of_objective(s32 i){return objective_state[i];}
DIFFICULTY lvlGetSelectedDifficulty(void){return DIFFICULTY_SECRET;}
u8 *get_ptr_text_for_watch_breifing_page(WATCH_BRIEFING_PAGE p){return (u8 *)(p==BRIEFING_TITLE?"DAM\n":"Original briefing text for the touch display.");}
int bondinvCountTotalItemsInInv(void){return 2;}
u16 *bondinvGetFirstTitlebyIndex(s32 i){return (u16 *)(i?"KF7 Soviet":"PP7 (silenced)");}
u16 *bondinvGetSecondTitlebyIndex(s32 i){(void)i;return (u16 *)"";}
void trigger_watch_zoom(f32 final,f32 time){assert(final>0 && time==15);}
u16 sndGetSfxSlotFirstNaturalVolume(void){return effects;}
void sndApplyVolumeAllSfxSlot(u16 v){effects=v;}
void musicTrack2ApplySeqpVol(u16 v){music=v;}
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank,s16 sound,ALSoundState *state){(void)bank;(void)sound;return state;}
static void check_navigation(void) {
    struct NativeMenuState s={0};
    assert(native_menu_step(&s,EOG_Y,0,0,0,0,0)==NATIVE_MENU_PASS && !s.open);
    native_menu_step(&s,0,0,0,1,0,0);
    assert(native_menu_step(&s,EOG_Y,0,0,1,0,0)==NATIVE_MENU_CONSUME && s.open);
    assert(native_menu_step(&s,EOG_Y,0,0,1,0,0)==NATIVE_MENU_CONSUME && s.open);
    native_menu_step(&s,0,0,0,1,0,0);
    native_menu_step(&s,EOG_DDOWN,0,0,1,0,0);assert(s.selected==1);
    for(int i=0;i<30;i++)native_menu_step(&s,EOG_DDOWN,0,0,1,0,0);
    assert(s.selected>1 && s.selected<10);
    native_menu_step(&s,0,0,0,1,0,0);
    assert(native_menu_step(&s,EOG_DRIGHT,0,0,1,0,0)==NATIVE_MENU_CHANGE_RIGHT);
    native_menu_step(&s,0,0,0,1,0,0);
    assert(native_menu_step(&s,EOG_A,0,0,1,0,0)==NATIVE_MENU_CHANGE_RIGHT);
    native_menu_step(&s,EOG_B,0,0,1,0,0);assert(!s.open);
    native_menu_step(&s,0,0,0,1,0,0);
    assert(native_menu_step(&s,EOG_A,0,0,1,1,0)==NATIVE_MENU_CONSUME && s.open);
    assert(native_menu_step(&s,EOG_SELECT,0,0,1,0,0)==NATIVE_MENU_EXIT);
    native_menu_step(&s,0,0,0,0,0,0);assert(!s.open);
    assert(native_menu_step(&s,EOG_A,0,0,1,0,1)==NATIVE_MENU_EXIT);
    puts("PASS main menu: pointer entry/exit, Y shortcut, B back, A change, bounded stick/D-pad repeats, menu-only exit");
}
static void check_bottom_watch(void) {
    test_player.watch_animation_state=0;
    native_bottom_game_tick();assert(captured.kind==EOG_BOTTOM_GAME && captured.count==2);
    test_player_count=2;native_bottom_game_tick();
    assert(captured.kind==EOG_BOTTOM_GAME && !captured.count && !strcmp(captured.title,"LOCAL MULTIPLAYER"));
    test_player_num=1;strcpy(captured.title,"P1 SENTINEL");
    native_bottom_game_tick();native_bottom_watch_publish();assert(!strcmp(captured.title,"P1 SENTINEL"));
    test_player_num=0;test_player_count=1;native_bottom_game_tick();
    assert(!strcmp(captured.rows[0].label,"First objective") && !strcmp(captured.rows[1].label,"Second objective"));
    objective_state[1]=OBJECTIVESTATUS_FAILED;native_bottom_game_tick();assert(captured.rows[1].status==2);
    watch_screen_index=WATCH_INDEX_MISSION_STATUS;
    test_player.bondhealth=.5f;test_player.bondarmour=.25f;
    native_bottom_watch_publish();assert(captured.kind==EOG_BOTTOM_WATCH && captured.health==50 && captured.armour==25);
    bottom_command=(struct EogBottomCommand){200,EOG_BOTTOM_PAGE,1,0};command_pending=1;
    native_bottom_watch_publish();assert(watch_screen_index==1 && captured.count==2 && !strcmp(captured.rows[1].label,"KF7 Soviet"));
    bottom_command=(struct EogBottomCommand){201,EOG_BOTTOM_ACTIVATE,1,0};command_pending=1;injected=0;
    native_bottom_watch_publish();assert(g_curWatchItemIndex==1 && watch_inventory_cursor_pos==1 && injected==A_BUTTON);
    bottom_command=(struct EogBottomCommand){201,EOG_BOTTOM_PAGE,3,0};command_pending=1;
    native_bottom_watch_publish();assert(captured.count==9 && captured.rows[0].tag==3 && captured.rows[7].tag==NATIVE_WATCH_AUDIO && captured.rows[8].tag==NATIVE_WATCH_ADVANCED);
    int old=native_setting_crosshair();
    bottom_command=(struct EogBottomCommand){203,EOG_BOTTOM_ACTIVATE,8,1};command_pending=1;
    native_bottom_watch_publish();assert(native_watch_advanced && captured.context==403 && captured.count==17);
    assert(native_watch_option_y(native_watch_option_count()-1)<=200);
    const int grouped[]={15,14,10,13,22,23,21,16,17,24,18,19,12,20,11,NATIVE_WATCH_EXPERIMENTAL,NATIVE_WATCH_BACK};
    for(unsigned i=0;i<sizeof grouped/sizeof *grouped;i++)assert(captured.rows[i].tag==grouped[i]);
    bottom_command=(struct EogBottomCommand){403,EOG_BOTTOM_ACTIVATE,14,1};command_pending=1;
    native_bottom_watch_publish();assert(native_setting_crosshair()==(old+1)%6);
    bottom_command=(struct EogBottomCommand){403,EOG_BOTTOM_ACTIVATE,15,1};command_pending=1;
    native_bottom_watch_publish();assert(native_watch_experimental && captured.context==803 && captured.count==3);
    int visibility=native_setting_guard_visibility(),rate=native_setting_guard_ai_hz();
    bottom_command=(struct EogBottomCommand){803,EOG_BOTTOM_ACTIVATE,0,1};command_pending=1;
    native_bottom_watch_publish();assert(native_setting_guard_visibility()==!visibility);
    bottom_command=(struct EogBottomCommand){803,EOG_BOTTOM_ACTIVATE,1,1};command_pending=1;
    native_bottom_watch_publish();assert(native_setting_guard_ai_hz()!=rate);
    bottom_command=(struct EogBottomCommand){803,EOG_BOTTOM_BACK,0,1};command_pending=1;
    native_bottom_watch_publish();assert(!native_watch_experimental && native_watch_advanced && captured.context==403 && native_watch_nav.selected==15);
    bottom_command=(struct EogBottomCommand){403,EOG_BOTTOM_BACK,0,1};command_pending=1;injected=0;
    native_bottom_watch_publish();assert(!native_watch_advanced && !injected && captured.context==203 && captured.count==9);
    bottom_command=(struct EogBottomCommand){203,EOG_BOTTOM_ACTIVATE,7,1};command_pending=1;
    native_bottom_watch_publish();assert(native_watch_audio && captured.context==603 && captured.count==4);
    assert(captured.rows[0].tag==1 && captured.rows[1].tag==2 && captured.rows[2].tag==25);
    int mixer=native_setting_audio_mixer();
    bottom_command=(struct EogBottomCommand){603,EOG_BOTTOM_ACTIVATE,2,1};command_pending=1;
    native_bottom_watch_publish();assert(native_setting_audio_mixer()==!mixer);
    bottom_command=(struct EogBottomCommand){603,EOG_BOTTOM_BACK,0,0};command_pending=1;
    native_bottom_watch_publish();assert(!native_watch_audio && captured.context==203 && native_watch_nav.selected==7);
    bottom_command=(struct EogBottomCommand){203,EOG_BOTTOM_PAGE,4,0};command_pending=1;
    native_bottom_watch_publish();assert(captured.count==5);
    bottom_command=(struct EogBottomCommand){204,EOG_BOTTOM_ACTIVATE,0,0};command_pending=1;
    native_bottom_watch_publish();assert(captured.rows[0].tag==2999 && captured.count>1);
    bottom_command=(struct EogBottomCommand){204,EOG_BOTTOM_BACK,0,0};command_pending=1;
    native_bottom_watch_publish();assert(captured.count==5 && native_bottom_brief==-1);
    // Touch points use the original page coordinates, not custom list rows.
    watch_screen_index=WATCH_INDEX_GAME_OPTIONS;watch_item_is_actively_selected=0;
    bottom_command=(struct EogBottomCommand){203,EOG_BOTTOM_POINT,220,native_watch_option_y(8)};command_pending=1;
    native_bottom_watch_publish();assert(native_watch_advanced && captured.context==403);
    old=native_setting_crosshair();bottom_command=(struct EogBottomCommand){403,EOG_BOTTOM_POINT,220,native_watch_option_y(14)};command_pending=1;
    native_bottom_watch_publish();assert(native_setting_crosshair()==(old+1)%6);
    native_watch_options_reset();native_watch_nav.selected=8;watch_held=EOG_A;
    native_watch_options_input();assert(!native_watch_advanced);watch_held=0;native_watch_options_input();watch_held=EOG_A;
    native_watch_options_input();assert(native_watch_advanced && native_watch_nav.selected==0);
    int fps=native_setting_fps();native_watch_options_input();assert(native_setting_fps()==fps);
    watch_held=0;native_watch_options_input();watch_held=EOG_DDOWN;native_watch_options_input();assert(native_watch_nav.selected==1);
    watch_held=0;native_watch_options_input();watch_held=EOG_B;native_watch_options_input();assert(!native_watch_advanced && native_watch_nav.selected==8);
    watch_held=0;native_watch_options_input();
    native_watch_option_action(NATIVE_WATCH_ADVANCED,1);native_watch_nav.selected=1;
    watch_held=EOG_DRIGHT;native_watch_options_input();int aa=native_setting_aa();
    for(int i=0;i<5;i++)native_watch_options_input();
    assert(native_setting_aa()==aa);
    watch_held=0;native_watch_options_input();native_watch_option_back();
    watch_screen_index=WATCH_INDEX_CONTROL_OPTIONS;int style=native_menu_option_value(0);
    bottom_command=(struct EogBottomCommand){202,EOG_BOTTOM_POINT,220,26};command_pending=1;
    native_bottom_watch_publish();assert(native_menu_option_value(0)==(style+1)%6);
    watch_screen_index=WATCH_INDEX_INVENTORY;watch_inventory_text_y=24;
    bottom_command=(struct EogBottomCommand){201,EOG_BOTTOM_POINT,100,178};command_pending=1;injected=0;
    native_bottom_watch_publish();assert(g_curWatchItemIndex==1 && injected==A_BUTTON);
    watch_screen_index=WATCH_INDEX_MISSION_STATUS;watch_item_is_actively_selected=0;D_800409A4=1;
    bottom_command=(struct EogBottomCommand){200,EOG_BOTTOM_POINT,100,78};command_pending=1;
    native_bottom_watch_publish();assert(watch_item_is_actively_selected && D_800409A4==0);
    bottom_command=(struct EogBottomCommand){200,EOG_BOTTOM_BACK,0,1};command_pending=1;injected=0;
    native_bottom_watch_publish();assert(!watch_item_is_actively_selected && !injected);
    bottom_command=(struct EogBottomCommand){200,EOG_BOTTOM_BACK,0,1};command_pending=1;
    native_bottom_watch_publish();assert(injected==START_BUTTON);
    // Populate real original textMeasure inputs, then execute the original
    // objective-page source in collection mode. No watch navigation runs.
    static struct font font;for(int i=0;i<94;i++){font.chars[i].width=4;font.chars[i].height=6;font.chars[i].baseline=1;}
    ptrFontBankGothic=&font;ptrFontBankGothicChars=font.chars;
    watch_screen_index=WATCH_INDEX_INVENTORY;mission_brief_index=BRIEF_INDEX_Q;
    int selected=g_curWatchItemIndex;watch_item_is_actively_selected=1;test_player.watch_animation_state=0;
    native_bottom_game_tick();assert(captured.objectives.count==7);
    assert(watch_screen_index==WATCH_INDEX_INVENTORY && mission_brief_index==BRIEF_INDEX_Q && g_curWatchItemIndex==selected && watch_item_is_actively_selected==1);
    assert(strstr(captured.objectives.text[1].text,"a: First objective"));
    assert(strstr(captured.objectives.text[3].text,"b: Second objective"));
    assert(captured.objectives.text[1].x==60 && captured.objectives.text[2].x==175);
    assert(strstr(captured.objectives.text[4].text,"Failed"));
    assert(captured.objectives.text[5].colour==0xa0ffa0f0 && !strcmp(captured.objectives.text[5].text,"OBJECTIVES\n"));
    assert(native_objective_collecting==0 && native_objective_layout==NULL);
    ptrFontBankGothic=NULL;ptrFontBankGothicChars=NULL;
    puts("PASS closed-watch objective snapshot uses original watch layout, letters/status columns, colours and difficulty without changing watch/navigation state");
    puts("PASS original bottom/watch bridge: difficulty-filtered live objectives, health/armour, all watch tabs, inventory equip pulse, shared options and full briefing navigation");
}
int main(void) {
    check_navigation();
    native_settings_init(NULL);check_bottom_watch();
    // Entering options with a held side key or stale selection cannot edit music.
    watch_screen_index=WATCH_INDEX_GAME_OPTIONS;watch_item_is_actively_selected=1;
    watch_held=EOG_R;native_watch_options_reset();native_watch_options_input();
    assert(watch_screen_index==WATCH_INDEX_GAME_OPTIONS && !watch_item_is_actively_selected);
    int before_volume=native_menu_option_value(1);
    watch_held=0;native_watch_options_input();watch_held=EOG_DRIGHT;native_watch_options_input();
    assert(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING && native_menu_option_value(1)==before_volume);
    watch_screen_index=WATCH_INDEX_GAME_OPTIONS;native_watch_options_reset();watch_held=0;native_watch_options_input();
    native_watch_option_action(NATIVE_WATCH_AUDIO,1);
    watch_held=EOG_A;native_watch_options_input();assert(native_watch_editing);
    watch_held=0;native_watch_options_input();watch_held=EOG_DLEFT;native_watch_options_input();
    assert(native_menu_option_value(1)<before_volume && watch_screen_index==WATCH_INDEX_GAME_OPTIONS);
    watch_held=0;native_watch_options_input();watch_held=EOG_B;native_watch_options_input();assert(!native_watch_editing);
    watch_held=0;native_watch_options_input();watch_held=EOG_A;native_watch_options_input();assert(native_watch_editing);
    assert(native_watch_option_back() && !native_watch_editing && !watch_item_is_actively_selected);
    watch_held=0;native_watch_options_input();watch_held=EOG_L;native_watch_options_input();
    assert(watch_screen_index==WATCH_INDEX_CONTROL_OPTIONS);
    // Enter from a horizontal tab gesture, then move directly upward:
    // a held vertical direction must not keep the neutral latch stuck.
    watch_held=0;watch_screen_index=WATCH_INDEX_GAME_OPTIONS;stick_x=80;stick_y=0;
    native_watch_options_reset();native_watch_options_input();assert(native_watch_wait_neutral);
    stick_x=0;stick_y=80;native_watch_options_input();assert(!native_watch_wait_neutral);
    native_watch_options_input();assert(native_watch_nav.selected==8);
    stick_y=0;native_watch_options_input();stick_x=80;native_watch_options_input();
    assert(watch_screen_index==WATCH_INDEX_MISSION_BRIEFING);
    stick_x=stick_y=0;
    watch_held=0;native_settings_init(NULL);set_mTrack2Vol(32767);effects=32767;

    char directory[]="/tmp/eog-shared-options-XXXXXX";assert(mkdtemp(directory));
    native_settings_init(directory);native_setting_set_style(8);native_setting_set_aspect(0);
    native_menu_option_change(0,1);assert(native_setting_style()==9 && test_player.cur_player_control_type_0==9);
    native_menu_option_change(11,-1);assert(native_setting_crosshair()==5);
    native_menu_option_change(12,-1);assert(native_setting_watch()==5);
    native_menu_option_change(13,1);assert(!native_setting_aa());
    native_menu_option_change(14,-1);assert(native_setting_fps()==30);
    native_menu_option_change(15,1);assert(native_setting_sensitivity()==125);
    native_menu_option_change(16,1);assert(native_setting_sharpness()==1);
    char unused_filter[32];
    assert(!strcmp(native_menu_option_label(16),"Image filter\n"));
    for(int mode=2;mode<=6;mode++)native_menu_option_change(16,1);
    assert(native_setting_sharpness()==6);
    assert(!strcmp(native_menu_option_text(16,unused_filter,sizeof unused_filter),"Smooth high\n"));
    native_menu_option_change(17,1);native_menu_option_change(18,-1);native_menu_option_change(19,1);
    assert(native_setting_grain()==1);
    native_menu_option_change(20,1);assert(native_setting_skin()==1);
    assert(!strcmp(native_menu_option_text(20,unused_filter,sizeof unused_filter),"Field\n"));
    native_menu_option_change(21,1);assert(native_setting_glass()==0);
    native_menu_option_change(21,1);assert(native_setting_glass()==1);
    assert(native_setting_vignette()==1 && native_setting_bloom()==2);
    native_menu_option_change(1,-1);native_menu_option_change(2,-1);
    assert(music==31129 && effects==31129);
    for(int i=0;i<8;i++){native_menu_option_change(3+i,1);assert(native_setting_game(i)>=0);}
    // Watch selections write the same preferences, not a separate menu copy.
    game_option_select_value(&game_options_entries[PLAYER_OPTION_AUTOAIM].current_value,1);
    assert(native_setting_game(1)==1);
    char unused[32];assert(!strcmp(native_menu_option_text(NATIVE_MENU_OPTIONS_COUNT,unused,sizeof unused),"\n"));
    native_menu_option_change(22,1);assert(native_setting_lod()==2);
    native_menu_option_change(23,1);assert(native_setting_view_distance()==3);
    assert(!strcmp(native_menu_option_text(23,unused,sizeof unused),"Near\n"));
    native_menu_option_change(22,1);native_menu_option_change(22,1);
    assert(native_setting_lod()==4 && !strcmp(native_menu_option_text(22,unused,sizeof unused),"Dynamic\n"));
    native_setting_dynamic_lod(3);assert(native_setting_lod_strength()==3 && native_setting_lod()==4);
    native_menu_option_change(24,1);native_menu_option_change(24,1);native_menu_option_change(24,1);
    assert(native_setting_vignette_mode()==3 && !strcmp(native_menu_option_text(24,unused,sizeof unused),"Crouch/Zoom\n"));
    native_menu_option_change(25,1);assert(native_setting_audio_mixer()==0);
    assert(!native_setting_unlock_all());
    native_menu_option_change(26,1);assert(native_setting_unlock_all());
    assert(!strcmp(native_menu_option_label(26),"Unlock everything\n"));
    assert(!strcmp(native_menu_option_text(26,unused,sizeof unused),"On\n"));
    native_menu_option_change(27,1);assert(native_setting_guard_visibility());
    native_menu_option_change(28,1);assert(native_setting_guard_ai_hz()==15);
    native_menu_option_change(28,1);assert(native_setting_guard_ai_hz()==5);
    native_menu_option_change(28,1);assert(native_setting_guard_ai_hz()==30);
    native_menu_option_change(28,-1);assert(native_setting_guard_ai_hz()==5);
    assert(!strcmp(native_menu_option_text(28,unused,sizeof unused),"5 Hz\n"));
    int expected[NATIVE_MENU_OPTIONS_COUNT];for(int i=0;i<NATIVE_MENU_OPTIONS_COUNT;i++)expected[i]=native_menu_option_value(i);
    native_settings_init(directory);
    for(int i=0;i<NATIVE_MENU_OPTIONS_COUNT;i++){assert(native_menu_option_value(i)==expected[i]);char b[32];assert(native_menu_option_text(i,b,sizeof b));}
    native_menu_option_change(26,-1);assert(!native_setting_unlock_all());
    native_settings_init(directory);assert(!native_setting_unlock_all());
    for(int i=0;i<8;i++)game_options_entries[i].current_value=0;
    assert(get_cur_player_look_vertical_inverted()==native_setting_game(0));
    assert(cur_player_get_autoaim()==native_setting_game(1));
    assert(cur_player_get_aim_control()==native_setting_game(2));
    assert(cur_player_get_sight_onscreen_control()==native_setting_game(3));
    assert(cur_player_get_lookahead()==native_setting_game(4));
    assert(cur_player_get_ammo_onscreen_setting()==native_setting_game(5));
    assert(cur_player_get_screen_setting()==native_setting_game(6));
    assert(get_screen_ratio()==native_setting_game(7));
    assert(!strcmp(native_colour_name(0,5),"Blue\n") && !strcmp(native_colour_name(1,4),"Blue\n"));
    // Legacy slot restore must not overwrite choices made in either native UI.
    cur_player_set_autoaim(0);assert(cur_player_get_autoaim()==1);
    set_screen_ratio(0);assert(get_screen_ratio()==native_setting_aspect());
    set_mTrack2Vol(0);assert(music==31129);
    effects=0;native_menu_options_restore_audio();assert(effects==31129);
    sub_GAME_7F0A91A0(0);assert(effects==31129);
    char path[512];snprintf(path,sizeof path,"%s/native-options.ini",directory);assert(!unlink(path));assert(!rmdir(directory));
    puts("PASS shared original watch/main options: all 29 choices survive reload, blue palettes, audio application, legacy save priority");
}
s16 viGetX(void){return 320;}
s16 viGetY(void){return 240;}
#undef sprintf
#include <stdarg.h>
int ge_sprintf(char *out,const char *format,...) {
    va_list args;va_start(args,format);int n=vsprintf(out,format,args);va_end(args);return n;
}
