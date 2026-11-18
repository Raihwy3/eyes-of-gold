#ifndef NATIVE_DERIVED_H
#define NATIVE_DERIVED_H
#include "native_convert.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct NativeBakedLodRecord {
  unsigned hash, count;
  const short *mid, *far, *distant;
};
struct NativeRoomLodRecord {
  unsigned room, hash, count;
  const short *mid, *far, *distant;
};
extern struct NativeBakedLodRecord *native_baked_lods;
extern struct NativeRoomLodRecord *native_room_lods;
extern unsigned native_baked_lod_count, native_room_lod_count;
extern unsigned char *original_bottom_background;
int native_derived_valid(const char *directory);
int native_derived_generate(const char *rom, const char *output,
                            NativeConvertProgress progress);
int native_derived_mount(const char *directory);
void native_derived_unmount(void);
#ifdef __cplusplus
}
#include "native_mesh_lod.h"
extern NativeMeshLod::Entry *native_bg_lods;
extern NativeSceneryBatches::Entry *native_room_packets;
extern unsigned native_bg_lod_count, native_room_packet_count;
#endif
#endif
