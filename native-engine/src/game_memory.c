/* Original runtime storage that the desktop fork placed at absolute addresses.
 * Native ARM pointers already fit in the game's 32-bit address fields. */
#include "native_ram.h"
unsigned char animations_frame_buffer[0x2d0] __attribute__((aligned(16)));
float __libm_qnan_f=__builtin_nanf("");
unsigned char (*tlbmanageGetTlbAllocatedBlock(void))[0x2000]
{
    return (unsigned char (*)[0x2000])(native_ram+NATIVE_HEAP_END);
}
