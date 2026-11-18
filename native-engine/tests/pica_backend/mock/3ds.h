#pragma once
#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/gpu/gpu.h>
#include <3ds/gpu/gx.h>
#include <3ds/gpu/shaderProgram.h>
#include <3ds/gfx.h>
#include <3ds/services/gspgpu.h>
#include <cstdlib>
#define SYSCLOCK_ARM11 268123480u
static inline u64 svcGetSystemTick(){static u64 tick;return ++tick;}
static inline void *linearAlloc(size_t n){return malloc(n);}
static inline void linearFree(void *p){free(p);}

static inline float osGet3DSliderState(void){return 0.f;}
