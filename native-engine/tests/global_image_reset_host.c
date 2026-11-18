/* Execute original texReset with recording texture consumers. The separate ARM
 * compile check verifies the same relocation path is selected on the console. */
#include <ultra64.h>
#include "game/image_bank.h"
#include "game/image.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static u8 storage[0x3000] __attribute__((aligned(4096)));
static FILE *cart;
static unsigned dl_calls, image_calls;
static const unsigned dl_offsets[] = {
    0,0x78,0x120,0x1c8,0x270,0x318,0x3c0,0x468,0x510,
    0x5b8,0x660,0x708,0x7b0,0x858,0x900,0x9a8,0xa50
};
void *mempAllocBytesInBank(u32 size, u8 bank) {
    (void)bank;
    if (size > sizeof storage) abort();
    memset(storage,0,sizeof storage);
    return storage;
}
void romCopy(void *dest, void *source, u32 size) {
    uintptr_t address=(uintptr_t)source;
    if (address != 0x1029d160 || size != 0x13f8) abort();
    if (fseek(cart, address-0x10000000, SEEK_SET) || fread(dest,1,size,cart)!=size) abort();
}
void texLoadFromDisplayList(Gfx *gdl, struct texpool *pool) {
    if (pool || dl_calls >= 17 || (u8*)gdl != storage+dl_offsets[dl_calls]) abort();
    dl_calls++;
}
void texLoad(s32 *entry, struct texpool *pool) {
    unsigned offset=image_calls==0 ? 0xac8 : image_calls<=6 ? 0xbc4+(image_calls-1)*12 : 0xc0c+(image_calls-7)*12;
    if (pool || image_calls>=12 || (u8*)entry != storage+offset) abort();
    image_calls++;
}
int main(int argc, char **argv) {
    if (argc!=2 || !(cart=fopen(argv[1],"rb"))) return 1;
    for (unsigned pass=0;pass<2;pass++) {
        dl_calls=image_calls=0;
        texReset();
        if (dl_calls!=17 || image_calls!=12 || (u8*)pGlobalimagetable!=storage) abort();
        if ((u8*)genericimage!=storage+0xac8 || (u8*)impactimages!=storage+0xad4 || (u8*)mainfolderimages!=storage+0xfd8 || (u8*)mpstageselimages!=storage+0x132c) abort();
    }
    fclose(cart);
    puts("PASS original global texture reset: 17 display lists, 12 preload entries and repeated stage reset stay within relocated bank");
    return 0;
}
