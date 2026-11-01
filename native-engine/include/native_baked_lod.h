#ifndef EOG_NATIVE_BAKED_LOD_H
#define EOG_NATIVE_BAKED_LOD_H
#include <ultra64.h>
#define NATIVE_BAKED_LOD_COMMAND 0x4b
struct NativeBakedLodPacket {const void *base;const short *xyz;unsigned count;};
#define native_baked_lod_command(pkt,params) gDma0p((pkt),NATIVE_BAKED_LOD_COMMAND,(params),(params)?1:0)
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned native_object_lod_tier __attribute__((weak));
extern Gfx *native_baked_lod_begin(Gfx *,const void *,unsigned) __attribute__((weak));
extern void native_baked_lod_reset(void) __attribute__((weak));
extern Gfx *native_room_lod_begin(Gfx *,unsigned,const void *,unsigned,unsigned) __attribute__((weak));
#ifdef __cplusplus
}
#endif
#endif
