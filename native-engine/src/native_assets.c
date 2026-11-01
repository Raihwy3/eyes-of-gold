/* Native-width sidecars use file-backed cart tokens, never mapped ROM memory.
 */
// clang-format off
#include <ultra64.h>
#include "native_assets.h"
#include "native_stage_limits.h"
#include "game/ob.h"
#include "native_cart.h"
#include "system.h"
#include <stdio.h>
#include <string.h>
// clang-format on
extern fileentry file_resource_table[];
extern resource_lookup_data_entry resource_lookup_data_array[];
extern s32 file_entry_max;
struct asset_row {
  char name[64];
  u32 offset, packed, raw, hash;
};
static struct asset_row rows[600];
static unsigned count, patched;
static uint32_t skin_rng;
static unsigned hash_update(unsigned h, const unsigned char *p, unsigned n) {
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
int native_assets_mount(const char *directory) {
  char path[512], cart[512];
  snprintf(path, sizeof path, "%s/native-game.dat", directory);
  snprintf(cart, sizeof cart, "%s/native-cart.bin", directory);
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  char magic[8];
  u32 header[4];
  unsigned char block[8192];
  int ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "EOGGAME1", 8) &&
           fread(header, 4, 4, f) == 4 && header[0] == 1 && header[1] > 3 &&
           header[1] <= 600;
  unsigned h = 2166136261u, bytes = 0;
  if (ok) {
    count = header[1];
    bytes = count * sizeof rows[0];
    ok = fread(rows, sizeof rows[0], count, f) == count;
    h = hash_update(h, (u8 *)rows, bytes);
    while (ok) {
      unsigned n = fread(block, 1, sizeof block, f);
      h = hash_update(h, block, n);
      bytes += n;
      if (n < sizeof block) {
        ok = !ferror(f);
        break;
      }
    }
    ok = ok && h == header[2];
  }
  fclose(f);
  if (!ok)
    return 0;
  for (unsigned i = 0; i < count; i++) {
    struct asset_row *r = &rows[i];
    if (!memchr(r->name, 0, 64) || r->offset < 24 + count * 80 ||
        r->offset > bytes + 24 || r->packed > bytes + 24 - r->offset ||
        !r->packed)
      return 0;
  }
  f = fopen(cart, "rb");
  if (!f)
    return 0;
  h = 2166136261u;
  unsigned cart_bytes = 0;
  while (1) {
    unsigned n = fread(block, 1, sizeof block, f);
    h = hash_update(h, block, n);
    cart_bytes += n;
    if (n < sizeof block) {
      ok = !ferror(f);
      break;
    }
  }
  fclose(f);
  if (!ok || h != header[3] || cart_bytes != 0xc00000u)
    return 0;
  native_cart_reset();
  if (!native_cart_mount(cart, 0x10000000u, 0, cart_bytes) ||
      !native_cart_mount(path, 0x14000000u, 0, bytes + 24))
    return 0;
  /* Animation frames are tiny per-actor ROM reads on every tick, just like
   * audio sample DMA. One immutable extent covers both and the tables between. */
  const unsigned hot_start=0x10124ac0u,hot_end=0x10419790u;
  int cached=native_cart_cache(hot_start,hot_end-hot_start);
  sysLogPrintf(2,"Animation/audio cartridge cache: %s (%u bytes)",
               cached?"RAM":"SD fallback",hot_end-hot_start);
  // The converted model/setup bank is small (~1.5 MiB). Cache it before
  // workers start so first appearances do not serialize tiny model DMAs on SD.
  int models_cached=native_cart_cache(0x14000000u,bytes+24);
  sysLogPrintf(2,"Converted model/setup cache: %s (%u bytes)",models_cached?"RAM":"SD fallback",bytes+24);
  patched = 0;skin_rng=0;
  sysLogPrintf(2,
               "Native game assets: %u models/stage files, verified original "
               "cart %u bytes",
               count, cart_bytes);
  return 1;
}
static void patch(void) {
  if (patched)
    return;
  if (!count)
    sysFatalError("Original game asset table was not mounted");
  unsigned matched = 0;
  for (unsigned i = 0; i < count; i++) {
    struct asset_row *r = &rows[i];
    if(!strncmp(r->name,"@gold/",6))continue;
    int found = 0;
    for (int j = 1; j < file_entry_max; j++) {
      if (file_resource_table[j].filename &&
          !strcmp(r->name, file_resource_table[j].filename)) {
        file_resource_table[j].hw_address =
            (u8 *)(uintptr_t)(0x14000000u + r->offset);
        resource_lookup_data_array[j].rom_size = r->packed;
        resource_lookup_data_array[j].pc_size = 0;
        resource_lookup_data_array[j].rom_remaining = 0;
        found = 1;
        matched++;
        break;
      }
    }
    if (!found)
      sysFatalError("Native game asset has no original resource entry: %s",
                    r->name);
  }
  patched = 1;
  sysLogPrintf(2, "Original resource table patched: %u entries", matched);
}
#include "native_rare_skin.h"
int native_assets_stage_skin(int mission,uint64_t entropy) {
  patch();int golden=0;
  if(mission) {
    if(!skin_rng){skin_rng=(uint32_t)entropy^(uint32_t)(entropy>>32)^0x9e3779b9u;if(!skin_rng)skin_rng=1;}
    do {golden=native_skin_outcome(native_skin_next(&skin_rng));}while(golden<0);
  }
  const char *names[]={"GwppksilZ","PchrwppksilZ"};
  const struct asset_row *chosen[2]={NULL,NULL};int ids[2]={0,0};
  for(unsigned n=0;n<2;n++) {
    char variant[64];snprintf(variant,sizeof variant,"%s%s",golden?"@gold/":"",names[n]);
    for(unsigned i=0;i<count;i++)if(!strcmp(rows[i].name,variant)){chosen[n]=&rows[i];break;}
    for(int j=1;j<file_entry_max;j++)if(file_resource_table[j].filename && !strcmp(file_resource_table[j].filename,names[n])){ids[n]=j;break;}
  }
  if(!chosen[0] || !chosen[1] || !ids[0] || !ids[1])sysFatalError("Missing silenced PP7 skin variant");
  for(unsigned n=0;n<2;n++) {
    file_resource_table[ids[n]].hw_address=(u8 *)(uintptr_t)(0x14000000u+chosen[n]->offset);
    resource_lookup_data_array[ids[n]].rom_size=chosen[n]->packed;
    resource_lookup_data_array[ids[n]].pc_size=resource_lookup_data_array[ids[n]].rom_remaining=0;
  }
  sysLogPrintf(2,"Mission PP7 finish: %s",golden?"Gold (1% roll)":"Original");
  return golden;
}
void pcmodelsPatchTable(void) { patch(); }
void pccgPatchTable(void) { patch(); }

/* Whole-part maps are baked from the verified cartridge during the build.
 * They contain coordinates only: original UVs, normals, colours and GBI stay. */
#include "native_baked_lod.h"
#include "dyn.h"
#ifdef __3DS__
#include "native_derived.h"
#else
struct NativeBakedLodRecord {unsigned hash,count;const short *mid,*far,*distant;};
#include "native_object_lods.h"
#endif
unsigned native_object_lod_tier;
static struct NativeBakedLookup {const void *base;unsigned count;const struct NativeBakedLodRecord *record;} native_baked_lookup[256];
#ifndef __3DS__
struct NativeRoomLodRecord {unsigned room,hash,count;const short *mid,*far,*distant;};
#include "native_room_lods.h"
#endif
static struct NativeRoomLodLookup {const void *base;unsigned count;const struct NativeRoomLodRecord *record;} native_room_lookup[NATIVE_ROOM_CAPACITY];
void native_baked_lod_reset(void){memset(native_baked_lookup,0,sizeof native_baked_lookup);memset(native_room_lookup,0,sizeof native_room_lookup);native_object_lod_tier=0;}
Gfx *native_room_lod_begin(Gfx *gdl,unsigned room,const void *base,unsigned count,unsigned tier) {
    if(!tier || !base || !count || room>=NATIVE_ROOM_CAPACITY)return gdl;
    struct NativeRoomLodLookup *lookup=&native_room_lookup[room];
    if(lookup->base!=base || lookup->count!=count) {
        lookup->base=base;lookup->count=count;lookup->record=NULL;
        for(unsigned i=0;i<native_room_lod_count;i++) {
            const struct NativeRoomLodRecord *r=&native_room_lods[i];
            if(r->room==room && r->count==count && r->hash==hash_update(2166136261u,base,count*16)){lookup->record=r;break;}
        }
    }
    if(!lookup->record)return gdl;
    struct NativeBakedLodPacket *packet=dynAllocate(sizeof *packet);
    packet->base=base;packet->count=count;packet->xyz=tier==1?lookup->record->mid:tier==2?lookup->record->far:lookup->record->distant;
    native_baked_lod_command(gdl++,packet);return gdl;
}
Gfx *native_baked_lod_begin(Gfx *gdl,const void *base,unsigned count) {
    if(!native_object_lod_tier || !base || !count || count>2048)return gdl;
    struct NativeBakedLookup *lookup=&native_baked_lookup[((uintptr_t)base>>4)%256];
    if(lookup->base!=base || lookup->count!=count) {
        unsigned h=hash_update(2166136261u,base,count*16);
        lookup->base=base;lookup->count=count;lookup->record=NULL;
        for(unsigned i=0;i<native_baked_lod_count;i++)if(native_baked_lods[i].hash==h && native_baked_lods[i].count==count){lookup->record=&native_baked_lods[i];break;}
    }
    if(!lookup->record)return gdl;
    struct NativeBakedLodPacket *packet=dynAllocate(sizeof *packet);
    packet->base=base;packet->count=count;packet->xyz=native_object_lod_tier==1?lookup->record->mid:native_object_lod_tier==2?lookup->record->far:lookup->record->distant;
    native_baked_lod_command(gdl++,packet);return gdl;
}

/* Load generations are captured into each frame's command stream. The render
 * worker owns its binding cache; no cross-worker pointers or mutable caches. */
#include "native_room_packets.h"
static unsigned native_room_packet_generations[NATIVE_ROOM_CAPACITY];
void native_room_packets_loaded(unsigned room) {
    if(room<NATIVE_ROOM_CAPACITY && ++native_room_packet_generations[room]==0)++native_room_packet_generations[room];
}
Gfx *native_room_packets_begin(Gfx *gdl,unsigned room,const void *base,unsigned bytes) {
    if(room>=NATIVE_ROOM_CAPACITY || !base || bytes<sizeof(Gfx) || !native_room_packet_generations[room])return gdl;
    struct NativeRoomPacketScope *p=dynAllocate(sizeof *p);
    p->base=base;p->bytes=bytes;p->room=room;p->generation=native_room_packet_generations[room];
    gDma0p(gdl++,NATIVE_ROOM_PACKET_COMMAND,p,1);return gdl;
}
