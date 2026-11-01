#ifndef EOG_NATIVE_MENU_FOCUS_H
#define EOG_NATIVE_MENU_FOCUS_H
/* Called by HID sampling before bottom-menu input arbitration. */
void native_bottom_menu_stick(int x,int y) __attribute__((weak));
#endif
