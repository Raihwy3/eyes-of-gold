#ifndef EOG_NATIVE_SUBSISTENCE_H
#define EOG_NATIVE_SUBSISTENCE_H
#include "native_settings.h"
extern s32 get_is_ramrom_flag(void);
extern s32 get_recording_ramrom_flag(void);
/* Intro loadout filtering only: pickups, scripts and objective items stay live. */
static inline int native_subsistence_enabled(int players,int mode,int demo) {
    return native_setting_subsistence() && players==1 && mode==GAMEMODE_SOLO && !demo;
}
static inline int native_subsistence_item(int item,int stage) {
    if(item==ITEM_UNARMED || item==ITEM_FIST || item==ITEM_WATCHLASER || item==ITEM_TRIGGER)return 1;
    if(item>=ITEM_BOMBCASE)return 1;
    return item==ITEM_REMOTEMINE && (stage==LEVELID_FACILITY || stage==LEVELID_SURFACE2);
}
static inline int native_subsistence_ammo(int ammo,int stage) {
    if(ammo>=AMMO_DARTS && ammo<AMMOTYPE_MAX && ammo!=AMMO_TANK)return 1;
    return ammo==AMMO_REMOTEMINE && (stage==LEVELID_FACILITY || stage==LEVELID_SURFACE2);
}
#endif
