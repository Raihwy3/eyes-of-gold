/* Type-only compatibility for existing port API declarations. No SDL runtime. */
#ifndef EOG3DS_SDL_TYPES_H
#define EOG3DS_SDL_TYPES_H
#include <stdint.h>
typedef int SDL_GameControllerButton;
typedef int SDL_GameControllerAxis;
#define SDL_NUM_SCANCODES 512
#endif
