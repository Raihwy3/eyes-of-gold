#ifndef EOG_NATIVE_CLIP_COLOUR_H
#define EOG_NATIVE_CLIP_COLOUR_H
#include <stdint.h>
/* Exact floor(value + .5) followed by 0..255 saturation for finite inputs.
 * Once saturated to a nonnegative range, float-to-integer truncation is floor.
 * Keep the addition in float, matching the original clipping arithmetic. */
static inline uint8_t native_clip_colour(float value) {
 float rounded=value+.5f;
 if(!(rounded>0.f))return 0;
 if(rounded>=255.f)return 255;
 return (uint8_t)rounded;
}
#endif
