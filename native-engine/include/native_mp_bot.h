#ifndef EOG_NATIVE_MP_BOT_H
#define EOG_NATIVE_MP_BOT_H
#ifdef __cplusplus
extern "C" {
#endif

struct MoveData;

enum {
    NATIVE_MP_BOT_MENU_NONE = 0,
    NATIVE_MP_BOT_MENU_FINISH_GAME = 1,
    NATIVE_MP_BOT_MENU_CLOSE_PAUSE = 2,
    NATIVE_MP_BOT_MENU_CONFIRM_QUIT = 3
};

/* Frontend option helpers (implemented in native_settings.c so bottom/front
 * UI checks do not pull gameplay AI engine dependencies). */
const char *native_mp_bot_label(void);
const char *native_mp_bot_value_text(void);
const char *native_mp_bot_status_note(void);
int native_mp_bot_enabled(void);
void native_mp_bot_toggle(void);

/* Gameplay bot predicates and AI driver (implemented in native_mp_bot.c). */
int native_mp_bot_is_active(int player_num, int player_count, int demo_active);
int native_mp_bot_should_respawn(int player_num, int player_count, int demo_active, int scenario, int deaths);
int native_mp_bot_menu_action(int player_num, int player_count, int demo_active, int game_over_flag, int menu_on, int menu_mode);
void native_mp_bot_reset(void);
void native_mp_bot_apply(struct MoveData *m);

#ifdef __cplusplus
}
#endif
#endif
