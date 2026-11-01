#ifndef NATIVE_GAME_BOOT_H
#define NATIVE_GAME_BOOT_H
int native_platform_console_model(void);
int native_game_start(const char *directory);
void native_game_request_quit(void);
int videoQuitRequested(void);
int native_game_render_idle(void);
#endif
