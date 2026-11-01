#ifndef EOG_NATIVE_PICKUP_H
#define EOG_NATIVE_PICKUP_H
/* Modern independent-look layouts retain automatic collection while looking
 * at floor items. Distance, inventory, settling and STAN obstruction checks
 * remain in the original objTickPlayer consumer. */
static inline int native_pickup_look_allowed(float pitch,int magnet,int style) {
    return (style==8 || style==9) || !(pitch<-.7853982f && magnet<0);
}
#endif
