#ifndef EOG_NATIVE_INPUT_H
#define EOG_NATIVE_INPUT_H
/* Platform device exit chord; START alone remains the original watch button. */
void native_input_init(void);
void native_input_suspend(void);
int native_input_exit_requested(void);
unsigned native_input_menu_buttons(void);
void native_input_press(unsigned buttons) __attribute__((weak));
void native_input_take_motion(int *x,int *y,unsigned *actions);
void native_input_take_touch_look(int *x,int *y) __attribute__((weak));
#endif
