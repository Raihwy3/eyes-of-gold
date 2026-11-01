#include "native_derived.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
struct TitleRange {
  unsigned offset, bytes, kind;
  unsigned char *destination;
};
#include "native_title_layout.h"
int main(int argc, char **argv) {
  if (argc < 3)
    return 2;
  if (!strcmp(argv[1], "generate")) {
    if (argc != 4)
      return 2;
    return native_derived_generate(argv[2], argv[3], nullptr) ? 0 : 1;
  }
  if (!strcmp(argv[1], "validate"))
    return native_derived_valid(argv[2]) ? 0 : 1;
  if (!strcmp(argv[1], "mount")) {
    if (!native_derived_mount(argv[2]))
      return 1;
    if (!original_bottom_background || !native_baked_lod_count ||
        !native_room_lod_count || !native_bg_lod_count ||
        !native_room_packet_count)
      return 3;
    const auto *title = original_bottom_background - 0x67f0;
    for (const auto &r : title_ranges)
      for (unsigned at = 0; at < r.bytes; at += 4) {
        unsigned expected;
        memcpy(&expected, title + r.offset + at, 4);
        if (r.kind == 3 && at % 8 == 4) {
          unsigned x;
          memcpy(&x, title + r.offset + at - 4, 4);
          unsigned op = x >> 24;
          if ((op == 3 || op == 4 || op == 6 || op == 0xfd) &&
              expected >> 24 == 2) {
            unsigned offset = expected & 0xffffff;
            bool found = false;
            for (const auto &target : title_ranges)
              if (offset >= target.offset &&
                  offset - target.offset < target.bytes) {
                expected = (unsigned)(uintptr_t)(target.destination + offset -
                                                 target.offset);
                found = true;
                break;
              }
            if (!found)
              return 7;
          }
        }
        unsigned actual;
        memcpy(&actual, r.destination + at, 4);
        if (actual != expected)
          return 8;
      }
    if (native_derived_mount(argv[2]))
      return 4;
    native_derived_unmount();
    if (original_bottom_background || native_baked_lod_count ||
        native_room_lods || native_bg_lods || native_room_packets)
      return 5;
    return native_derived_mount(argv[2]) ? (native_derived_unmount(), 0) : 6;
  }
  return 2;
}
