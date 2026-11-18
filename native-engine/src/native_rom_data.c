/* Original game definitions live in zero-filled storage. The user's verified
 * ROM supplies scalar values and pointer selection; the native address map
 * only adapts original addresses to this executable's ABI. */
#include "native_rom_data.h"
#include "native_inflate.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct NativeRomField {
  unsigned offset, width, count, stride, pointer, source, source_stride;
};
struct NativeRomObject {
  unsigned char *destination;
  unsigned bytes, source_bytes, raw_rom, source, first, fields, hash;
};
struct NativeRomPointer {
  unsigned key, kind;
  const void *(*get)(void);
  unsigned addend;
};
#include "native_rom_data_layout.h"
#ifdef NATIVE_ROM_CUSTOM_MENU_DATA
/* These IDs belong to our extra controls/options, not the retail menu data.
 * Original records have already been loaded and validated before this runs. */
static void custom_menu_defaults(void) {
  int32_t styles;
  memcpy(&styles, D_800409D8, sizeof styles);
  styles += 2;
  memcpy(D_800409D8, &styles, sizeof styles);
  for (unsigned i = 0; i < 2; i++) {
    uint16_t label = (uint16_t)(0xf000u + i);
    memcpy(game_control_styles + 160 + i * 20, &label, sizeof label);
  }
  for (unsigned i = 0; i < 4; i++) {
    unsigned char *entry = game_options_entries + 96 + i * 12;
    uint16_t label = (uint16_t)(0xf010u + i);
    uint32_t initial = i >= 2;
    memcpy(entry, &label, sizeof label);
    memcpy(entry + 8, &initial, sizeof initial);
  }
}
#endif
struct RomHuft {
  uint8_t e, b;
  union {
    uint16_t n;
    struct RomHuft *table;
  } v;
};
extern uint32_t decompressdata(void *, void *, struct RomHuft *);
static unsigned char original_cdata[247120];
static const char *error_message = "Original ROM data could not be prepared";
static int ready;
static unsigned be32(const unsigned char *p) {
  return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 |
         p[3];
}
static unsigned hash_bytes(const void *data, unsigned bytes) {
  const unsigned char *p = data;
  unsigned hash = 2166136261u;
  while (bytes--)
    hash = (hash ^ *p++) * 16777619u;
  return hash;
}
static int resolve(unsigned key, uint32_t *out) {
  if (!key) {
    *out = 0;
    return 1;
  }
  unsigned lo = 0,
           hi = sizeof native_rom_pointers / sizeof *native_rom_pointers;
  while (lo < hi) {
    unsigned mid = (lo + hi) / 2;
    if (native_rom_pointers[mid].key < key)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo >= sizeof native_rom_pointers / sizeof *native_rom_pointers ||
      native_rom_pointers[lo].key != key)
    return 0;
  const struct NativeRomPointer *p = &native_rom_pointers[lo];
  uintptr_t value;
  switch (p->kind) {
  case 0:
    value = key;
    break;
  case 1:
    value = 0x10000000u + key;
    break;
  case 2:
    if (p->addend >= sizeof original_cdata)
      return 0;
    value = (uintptr_t)(original_cdata + p->addend);
    break;
  case 3:
    if (!p->get)
      return 0;
    value = (uintptr_t)p->get() + p->addend;
    break;
  case 4:
    value = key & 0xffffffu;
    break;
  default:
    return 0;
  }
  if (value > UINT32_MAX)
    return 0;
  *out = (uint32_t)value;
  return 1;
}
const char *native_rom_data_error(void) { return error_message; }
unsigned native_rom_data_count(void) {
  return sizeof native_rom_objects / sizeof *native_rom_objects;
}
int native_rom_data_init(const char *directory) {
  if (ready)
    return 1;
  char name[512];
  int length = snprintf(name, sizeof name, "%s/native-cart.bin", directory);
  if (length < 0 || (unsigned)length >= sizeof name)
    return 0;
  FILE *f = fopen(name, "rb");
  if (!f)
    return 0;
  unsigned char *workspace = calloc(sizeof original_cdata + 64 + 71760 + 16, 1);
  struct RomHuft *hlist = calloc(NATIVE_HUFT_ENTRIES, sizeof *hlist);
  int ok = 0;
  if (!workspace || !hlist)
    goto done;
  unsigned char *input = workspace + sizeof original_cdata + 64;
  if (fseek(f, 0x21990, SEEK_SET) || fread(input, 1, 71760, f) != 71760 ||
      input[0] != 0x11 || input[1] != 0x72)
    goto done;
  if (decompressdata(input, workspace, hlist) != sizeof original_cdata ||
      hash_bytes(workspace, sizeof original_cdata) != NATIVE_ROM_CDATA_HASH) {
    error_message = "Original ROM data segment failed validation";
    goto done;
  }
  memcpy(original_cdata, workspace, sizeof original_cdata);
  for (unsigned i = 0; i < native_rom_data_count(); i++) {
    const struct NativeRomObject *r = &native_rom_objects[i];
    const unsigned char *source = NULL;
    unsigned char *raw = NULL;
    if (r->raw_rom) {
      raw = malloc(r->source_bytes ? r->source_bytes : 1);
      if (!raw)
        goto done;
      if (fseek(f, r->source, SEEK_SET) ||
          fread(raw, 1, r->source_bytes, f) != r->source_bytes) {
        free(raw);
        goto done;
      }
      source = raw;
    } else if (r->source > sizeof original_cdata ||
               r->source_bytes > sizeof original_cdata - r->source)
      goto done;
    if (!r->raw_rom)
      source = original_cdata + r->source;
    memset(r->destination, 0, r->bytes);
    for (unsigned j = 0; j < r->fields; j++) {
      const struct NativeRomField *field = &native_rom_fields[r->first + j];
      if (field->pointer == 1)
        continue;
      for (unsigned n = 0; n < field->count; n++) {
        unsigned at = field->offset + n * field->stride;
        unsigned input_at = field->source + n * field->source_stride;
        if (at > r->bytes || field->width > r->bytes - at ||
            input_at > r->source_bytes || field->width > r->source_bytes - input_at) {
          free(raw);
          goto done;
        }
        if (field->pointer == 2) {
          if (field->width != 4) { free(raw); goto done; }
          unsigned packed = be32(source + input_at), value = 0;
          for (unsigned nibble = 0; nibble < 4; nibble++)
            value |= ((packed >> (28 - nibble * 4)) & 15u) << (nibble * 4);
          memcpy(r->destination + at, &value, 4);
        } else if (field->pointer == 3) {
          /* Preserve the inherited port's default: manual look-ahead is off.
           * Original save defaults are otherwise taken from the ROM. */
          if (field->width != 2) { free(raw); goto done; }
          uint16_t value = ((uint16_t)source[input_at] << 8 | source[input_at + 1]) & ~(uint16_t)16;
          memcpy(r->destination + at, &value, sizeof value);
        } else {
          for (unsigned k = 0; k < field->width; k++)
            r->destination[at + k] = source[input_at + field->width - 1 - k];
        }
      }
    }
    if (hash_bytes(r->destination, r->bytes) != r->hash) {
      error_message =
          "Original ROM definition does not match the native layout";
      free(raw);
      goto done;
    }
    for (unsigned j = 0; j < r->fields; j++) {
      const struct NativeRomField *field = &native_rom_fields[r->first + j];
      if (field->pointer != 1)
        continue;
      for (unsigned n = 0; n < field->count; n++) {
        unsigned at = field->offset + n * field->stride;
        unsigned input_at = field->source + n * field->source_stride;
        uint32_t value;
        if (at > r->bytes || 4 > r->bytes - at ||
            input_at > r->source_bytes || 4 > r->source_bytes - input_at ||
            !resolve(be32(source + input_at), &value)) {
          error_message = "Original ROM definition has an unmapped pointer";
          free(raw);
          goto done;
        }
        memcpy(r->destination + at, &value, 4);
      }
    }
    free(raw);
  }
#ifdef NATIVE_ROM_CUSTOM_MENU_DATA
  custom_menu_defaults();
#endif
  ready = 1;
  ok = 1;
done:
  free(workspace);
  free(hlist);
  fclose(f);
  return ok;
}
