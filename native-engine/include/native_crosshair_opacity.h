#ifndef EOG_NATIVE_CROSSHAIR_OPACITY_H
#define EOG_NATIVE_CROSSHAIR_OPACITY_H
#include "native_settings.h"
/* Original is the retail sprite's 0x6e alpha. Explicit percentages use the
 * same conversion for the ROM sprite and custom silhouettes. */
static inline unsigned native_crosshair_alpha(void){
    int opacity=native_setting_crosshair_opacity();
    return opacity<0?0x6eu:(unsigned)(opacity*255+50)/100;
}
#endif
