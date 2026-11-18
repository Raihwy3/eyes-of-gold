/* Desktop reference assets are host-test-only; never linked into the app. */
#include "native_mesh_lod.h"
#include <cstdint>
#include <cstdio>
#include <vector>
struct NativeBakedLodRecord {
  unsigned hash, count;
  const short *mid, *far, *distant;
};
struct NativeRoomLodRecord {
  unsigned room, hash, count;
  const short *mid, *far, *distant;
};
#include "native_bg_lods.h"
#include "native_bottom_background.h"
#include "native_object_lods.h"
#include "native_room_lods.h"
#include "native_room_packets_data.h"
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  FILE *out = fopen(argv[1], "wb");
  if (!out)
    return 1;
  auto word = [&](unsigned v) { fwrite(&v, 4, 1, out); };
  fwrite("EOGDRAW1", 1, 8, out);
  word(native_baked_lod_count);
  word(native_room_lod_count);
  word(native_bg_lod_count);
  word(native_room_packet_count);
  word(0);
  word(0);
  for (const auto &r : native_baked_lods) {
    word(r.hash);
    word(r.count);
    for (auto *p : {r.mid, r.far, r.distant})
      fwrite(p, 6, r.count, out);
  }
  for (const auto &r : native_room_lods) {
    word(r.room);
    word(r.hash);
    word(r.count);
    for (auto *p : {r.mid, r.far, r.distant})
      fwrite(p, 6, r.count, out);
  }
  while (ftell(out) % 4)
    fputc(0, out);
  fwrite(native_bg_lods, sizeof *native_bg_lods, native_bg_lod_count, out);
  fwrite(native_room_packets, sizeof *native_room_packets,
         native_room_packet_count, out);
  fwrite(original_bottom_background, 1, 76800, out);
  return fclose(out) ? 1 : 0;
}
