#ifndef EOG_NATIVE_MENU_OPTIONS_H
#define EOG_NATIVE_MENU_OPTIONS_H
#define NATIVE_MENU_OPTIONS_COUNT 32
int native_menu_option_at(int index);
int native_menu_option_value(int row);
void native_menu_option_change(int row,int direction);
const char *native_menu_option_label(int row);
const char *native_menu_option_text(int row,char *buffer,unsigned size);
void native_menu_options_restore_audio(void);
#endif
