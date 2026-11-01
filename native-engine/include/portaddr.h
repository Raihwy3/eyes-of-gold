#ifndef PORT_PORTADDR_H
#define PORT_PORTADDR_H
#include "native_ram.h"
/* Runtime bases: ordinary allocated memory, no desktop fixed-address mirrors. */
#define PORT_DRAM_V1_BASE ((uintptr_t)native_ram)
#define PORT_DRAM_K0_BASE ((uintptr_t)native_ram)
#define PORT_DRAM_SIZE NATIVE_RAM_SIZE
#endif
