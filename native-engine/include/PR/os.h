#ifndef EOG_NATIVE_OS_H
#define EOG_NATIVE_OS_H
#include "include/PR/os.h"
#include "native_ram.h"
#include "portaddr.h"
#undef OS_K0_TO_PHYSICAL
#undef OS_PHYSICAL_TO_K0
#define OS_K0_TO_PHYSICAL(x) native_ram_translate_kseg((uintptr_t)(x))
#define OS_PHYSICAL_TO_K0(x) ((void *)native_ram_translate_kseg((uintptr_t)(x)))
#endif
