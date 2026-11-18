#ifndef NATIVE_DISPLAY_H
#define NATIVE_DISPLAY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Call once after the top console stops drawing and before C3D_Init. */
uint32_t native_display_prepare(void);
#ifdef __cplusplus
}
#endif
#endif
