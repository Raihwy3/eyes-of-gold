#ifndef NATIVE_GAME_ASSETS_H
#define NATIVE_GAME_ASSETS_H
#include <stdint.h>
int native_assets_mount(const char *directory);
int native_assets_stage_skin(int mission,uint64_t entropy) __attribute__((weak));
#endif
