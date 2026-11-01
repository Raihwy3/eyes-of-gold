/* Relocatable original RAM arena: no fixed mappings or mirrored virtual pages. */
#include "native_ram.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
unsigned char *native_ram;
int native_ram_init(void)
{
    if (native_ram) return 1;
    unsigned char *memory=memalign(4096,NATIVE_RAM_SIZE);
    if (!memory) return 0;
    /* Live pointers must be distinguishable from original physical offsets. */
    if ((uintptr_t)memory<NATIVE_RAM_SIZE) {free(memory);return 0;}
    memset(memory,0,NATIVE_RAM_SIZE);
    native_ram=memory;
    return 1;
}
void native_ram_destroy(void)
{
    free(native_ram);
    native_ram=NULL;
}
int native_ram_offset(const void *pointer,size_t bytes,uint32_t *offset)
{
    if (!native_ram || !offset) return 0;
    uintptr_t address=(uintptr_t)pointer,base=(uintptr_t)native_ram;
    if (address<base || address-base>NATIVE_RAM_SIZE) return 0;
    uintptr_t relative=address-base;
    if (bytes>NATIVE_RAM_SIZE-relative) return 0;
    *offset=(uint32_t)relative;
    return 1;
}
void *native_ram_resolve(uintptr_t address,size_t bytes)
{
    if (!native_ram) return NULL;
    uint32_t relative;
    if (native_ram_offset((const void *)address,bytes,&relative))
        return native_ram+relative;
    if (address>=0x80000000u && address<0x80800000u)
        address-=0x80000000u;
    else if (address>=0xa0000000u && address<0xa0800000u)
        address-=0xa0000000u;
    if (address>NATIVE_RAM_SIZE || bytes>NATIVE_RAM_SIZE-address) return NULL;
    return native_ram+address;
}
uintptr_t native_ram_translate_kseg(uintptr_t address)
{
    if (address>=0x80000000u && address<0xc0000000u) {
        uintptr_t physical=address&0x1fffffffu;
        if (physical<NATIVE_RAM_SIZE)
            return native_ram?(uintptr_t)(native_ram+physical):0;
        /* A few original PORT paths OR the KSEG bit onto an already-live
         * ARM pointer. Undo that tag without treating the pointer as an offset. */
        return physical;
    }
    return address;
}
