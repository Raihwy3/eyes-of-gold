#ifndef EOG_NATIVE_R4300_H
#define EOG_NATIVE_R4300_H
#include "include/PR/R4300.h"
#include "native_ram.h"
#undef PHYS_TO_K0
#undef PHYS_TO_K1
#undef K0_TO_PHYS
#undef K1_TO_PHYS
/* Display lists carry live ARM pointers. Explicit original KSEG tokens are
 * translated at the boundary; GPU physical addresses belong to the backend. */
#define PHYS_TO_K0(x) native_ram_translate_kseg((uintptr_t)(x))
#define PHYS_TO_K1(x) native_ram_translate_kseg((uintptr_t)(x))
#define K0_TO_PHYS(x) native_ram_translate_kseg((uintptr_t)(x))
#define K1_TO_PHYS(x) native_ram_translate_kseg((uintptr_t)(x))
#endif
