#ifndef EOG_NATIVE_RAM_H
#define EOG_NATIVE_RAM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NATIVE_RAM_SIZE 0x800000u
#define NATIVE_HEAP_START 0x50000u
#define NATIVE_HEAP_END 0x2f4400u
extern unsigned char *native_ram;
int native_ram_init(void);
void native_ram_destroy(void);
/* Accept a RAM offset, an original KSEG0/KSEG1 token, or a live arena pointer.
 * Segmented display-list addresses and cartridge addresses belong to separate
 * resolvers. A zero physical offset denotes the first byte of the arena. */
void *native_ram_resolve(uintptr_t address,size_t bytes);
int native_ram_offset(const void *pointer,size_t bytes,uint32_t *offset);
uintptr_t native_ram_translate_kseg(uintptr_t address);
#ifdef __cplusplus
}
#endif
#endif
