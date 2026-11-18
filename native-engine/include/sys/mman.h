/* 3DS pointers are already 32-bit. The desktop MAP_32BIT stack workaround
 * must stay disabled; fixed-address mmap is not available on this target. */
#ifndef __3DS__
#include_next <sys/mman.h>
#endif
