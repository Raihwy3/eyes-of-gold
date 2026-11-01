#include "native_scenery_scope.h"
#include "native_room_packets.h"
/* Native 32-bit BG boundary: original compressed room data, actual original
 * decompressor, image loader and display-list texture expansion. No mesh bake.
 */
#include "native_room.h"
#include "game/bg.h"
#include "game/image.h"
#include "game/tex.h"
#include "game/zlib.h"
#include "native_cart.h"
#include "native_graphics.h"
#include "native_motion.h"
#include "native_ram.h"
#include <bondtypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
extern void romCopy(void *, void *, u32);
extern void image_entries_load(void);
extern u32 decompressdata(u8 *, u8 *, struct huft *);
extern u8 _imagesSegmentRomStart[];
extern void sysLogPrintf(int, const char *, ...);
#define MAX_RAW 65536
#define EXPANDED 32768
static struct huft inflate_tables[4096];
static u8 compressed[MAX_RAW], raw[MAX_RAW];
static Gfx primary[EXPANDED], secondary[EXPANDED];
static Gfx native_commands[MAX_RAW / 8], frame_commands[64];
static Mtx model, projection;
static Vp viewport = {{{880, 660, 511, 0}, {880, 660, 511, 0}}};
static bg_room_data room;
static Vtx *vertices;
static struct texpool textures;
static unsigned header[24], primary_bytes, secondary_bytes;
static unsigned texture_count, triangle_count;
static float centre[3], radius;
static int loaded, images_initialized;
#if __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(bg_room_data) == 24, "native room record");
_Static_assert(sizeof(struct texpool) == 16, "native texture pool");
_Static_assert(sizeof(struct tex) == 16, "native texture metadata");
_Static_assert(sizeof(Gfx) == 8, "native GBI stride");
#endif
static unsigned be32(const u8 *p) {
  return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 |
         p[3];
}
static unsigned fnv(const u8 *p, unsigned bytes) {
  unsigned h = 2166136261u;
  for (unsigned i = 0; i < bytes; ++i)
    h = (h ^ p[i]) * 16777619u;
  return h;
}
static int verify(FILE *f, unsigned bytes, unsigned expected) {
  unsigned h = 2166136261u;
  while (bytes) {
    unsigned n = bytes > sizeof raw ? sizeof raw : bytes;
    if (fread(raw, 1, n, f) != n)
      return 0;
    for (unsigned i = 0; i < n; ++i)
      h = (h ^ raw[i]) * 16777619u;
    bytes -= n;
  }
  return h == expected;
}
static int expand(unsigned address, unsigned packed_bytes, unsigned bytes,
                  unsigned expected, u8 *out) {
  if (!address)
    return !packed_bytes && !bytes;
  unsigned offset = address & 0xffffff;
  if (address >> 24 != 15 || !packed_bytes ||
      packed_bytes > sizeof compressed || bytes > MAX_RAW ||
      offset > header[3] || packed_bytes > header[3] - offset)
    return 0;
  romCopy(compressed, (void *)(uintptr_t)(header[2] + offset), packed_bytes);
  if (compressed[0] != 0x11 || compressed[1] != 0x72)
    return 0;
  unsigned actual = decompressdata(compressed, out, inflate_tables);
  return actual == bytes && fnv(out, bytes) == expected;
}
static int convert_list(unsigned address, unsigned packed_bytes, unsigned bytes,
                        unsigned expected, Gfx *out, unsigned *written) {
  *written = 0;
  if (!address)
    return !bytes;
  if (bytes % 8 || !expand(address, packed_bytes, bytes, expected, raw))
    return 0;
  unsigned count = bytes / 8;
  int ended = 0;
  if (count > MAX_RAW / 8)
    return 0;
  for (unsigned i = 0; i < count; ++i) {
    unsigned a = be32(raw + i * 8), b = be32(raw + i * 8 + 4), op = a >> 24;
    native_commands[i].words.w0 = a;
    native_commands[i].words.w1 = b;
    if (op == 4) {
      unsigned n = (a & 65535) / 16, d = (a >> 16) & 15, off = b & 0xffffff;
      if (b >> 24 != 14 || d + n > 16 || off + n * 16 > header[6])
        return 0;
    }
    if (op == 0xc0 && ((b & 4095) > header[21] || (a & 7) == 1))
      return 0;
    if (op == 0xb8) {
      count = i + 1;
      ended = 1;
      break;
    }
  }
  if (!ended || count * 32 + 32 > EXPANDED)
    return 0;
  s32 size =
      texLoadFromGdl(native_commands, count * sizeof(Gfx), out, &textures);
  if (size <= 0 || size > EXPANDED * sizeof(Gfx) || size % sizeof(Gfx))
    return 0;
  *written = size;
  return 1;
}
int native_room_load(const char *path) {
  loaded = 0;
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  char magic[8];
  int ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "EOGROOM1", 8) &&
           fread(header, 4, 24, f) == 24;
  ok = ok && header[0] == 1 && header[1] == 135 && header[2] == 0x105ffc50 &&
       header[3] == 197024 && header[4] == (uintptr_t)_imagesSegmentRomStart &&
       header[5] == 0x308210 && header[23] == 0 && header[6] > 0 &&
       header[6] <= MAX_RAW && header[6] % 16 == 0 && header[7] <= MAX_RAW &&
       header[8] <= MAX_RAW && header[19] == 20 &&
       header[20] == 20 + 135 * 24 && header[21] < 4095;
  if (ok)
    ok = verify(f, header[3], header[12]) && verify(f, header[5], header[13]) &&
         fgetc(f) == EOF;
  fclose(f);
  if (!ok)
    return 0;
  if (!native_ram_init())
    return 0;
  native_cart_reset();
  if (!native_cart_mount(path, header[2], 104, header[3]) ||
      !native_cart_mount(path, header[4], 104 + header[3], header[5]))
    return 0;
  romCopy(raw, (void *)(uintptr_t)(header[2] + header[20]), 24);
  room.pPointTableBin = be32(raw);
  room.pPriMappingBin = be32(raw + 4);
  room.pSecMappingBin = be32(raw + 8);
  for (unsigned i = 0; i < 3; ++i) {
    unsigned bits = be32(raw + 12 + i * 4);
    memcpy((float *)&room.pos + i, &bits, 4);
  }
  vertices = (Vtx *)(native_ram + 0x40000);
  if (!expand(room.pPointTableBin, header[16], header[6], header[9],
              (u8 *)vertices))
    return 0;
  // Positions/flags/UVs are six native halfwords; RGBA stays byte ordered.
  for (unsigned i = 0; i < header[6]; i += 16)
    for (unsigned j = 0; j < 12; j += 2) {
      u8 *p = (u8 *)vertices + i + j;
      u8 t = p[0];
      p[0] = p[1];
      p[1] = t;
    }
  float lo[3] = {1e20f, 1e20f, 1e20f}, hi[3] = {-1e20f, -1e20f, -1e20f};
  for (unsigned i = 0; i < header[6] / 16; ++i)
    for (unsigned j = 0; j < 3; ++j) {
      float v = vertices[i].v.ob[j];
      if (v < lo[j])
        lo[j] = v;
      if (v > hi[j])
        hi[j] = v;
    }
  radius = 0;
  for (unsigned j = 0; j < 3; ++j) {
    centre[j] = (lo[j] + hi[j]) * .5f;
    float extent = hi[j] - lo[j];
    radius += extent * extent;
  }
  radius = sqrtf(radius) * .5f;
  if (!images_initialized) {
    image_entries_load();
    images_initialized = 1;
  }
  g_TexCacheCount = 0;
  texInitPool(&textures, native_ram + 0x300000, 0x400000);
  if (!convert_list(room.pPriMappingBin, header[17], header[7], header[10],
                    primary, &primary_bytes) ||
      !convert_list(room.pSecMappingBin, header[18], header[8], header[11],
                    secondary, &secondary_bytes))
    return 0;
  texture_count = (unsigned)(textures.end - textures.rightpos);
  triangle_count = header[14] + header[15];
  if (texture_count != header[22] || texFreeBytesInBuffer(&textures) < 8192)
    return 0;
  sysLogPrintf(2,
               "Original Dam room %u: %u vertices, %u triangles, %u textures; "
               "expanded lists %u/%u bytes",
               header[1], header[6] / 16, triangle_count, texture_count,
               primary_bytes, secondary_bytes);
  loaded = 1;
  return 1;
}
static void pack_matrix(Mtx *out, float m[4][4]) {
  unsigned *words = (unsigned *)out;
  memset(out, 0, sizeof *out);
  for (unsigned i = 0; i < 4; ++i)
    for (unsigned j = 0; j < 4; ++j) {
      int fixed = (int)(m[i][j] * 65536.f);
      unsigned shift = (j & 1) ? 0 : 16;
      words[i * 2 + j / 2] |= ((unsigned)fixed >> 16 & 65535) << shift;
      words[8 + i * 2 + j / 2] |= ((unsigned)fixed & 65535) << shift;
    }
}
void native_room_render(float yaw, float pitch, float distance) {
  if (!loaded)
    return;
  float scale = 2.f / (radius > 1 ? radius : 1), c = cosf(yaw), s = sinf(yaw),
        cp = cosf(pitch), sp = sinf(pitch);
  // Normalize before the view rotation: original local room vertices retain
  // their exact s16 coordinates. Only the harness's model/view matrices differ.
  float v[4][4] = {{c * scale, s * sp * scale, s * cp * scale, 0},
                   {0, cp * scale, -sp * scale, 0},
                   {-s * scale, c * sp * scale, c * cp * scale, 0},
                   {0, 0, 0, 1}};
  for (unsigned j = 0; j < 3; ++j)
    v[3][j] =
        -(centre[0] * v[0][j] + centre[1] * v[1][j] + centre[2] * v[2][j]);
  v[3][2] -= distance;
  float f = 1.f / tanf(.6f), near = .1f, far = 20.f;
  float p[4][4] = {{f / (4.f / 3.f), 0, 0, 0},
                   {0, f, 0, 0},
                   {0, 0, (far + near) / (near - far), -1},
                   {0, 0, 2 * far * near / (near - far), 0}};
  pack_matrix(&model, v);
  pack_matrix(&projection, p);
  Gfx *g = frame_commands;
  gDPPipeSync(g++);
  gSPViewport(g++, &viewport);
  gDPSetScissor(g++, G_SC_NON_INTERLACE, 0, 0, 440, 330);
  gSPClearGeometryMode(g++, 0xffffffff);
  gSPSetGeometryMode(g++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
  gDPSetTexturePersp(g++, G_TP_PERSP);
  gDPSetTextureFilter(g++, G_TF_POINT);
  gDPSetAlphaCompare(g++, G_AC_NONE);
  gSPMatrix(g++, &projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(g++, &model, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPSegment(g++, 0x0e, vertices);
  struct NativeRoomPacketScope program={primary,primary_bytes,135,1};
  native_scenery_scope(g++,1);
  gDma0p(g++,NATIVE_ROOM_PACKET_COMMAND,&program,1);
  gSPDisplayList(g++, primary);
  gDma0p(g++,NATIVE_ROOM_PACKET_COMMAND,0,0);
  native_scenery_scope(g++,0);
  if (secondary_bytes)
    gSPDisplayList(g++, secondary);
  gSPEndDisplayList(g++);
  native_graphics_render(frame_commands);
}
unsigned native_room_triangles(void) { return triangle_count; }
unsigned native_room_textures(void) { return texture_count; }

/* World bring-up: all original room streams share the original texture pool.
 * Portal adjacency is deliberately conservative here, not the game's complete
 * camera-clipped visibility traversal. Collision queries below are original C.
 */
#include "game/stan.h"
#include <stdlib.h>
struct StanPrefixRecord {
  s32 stanfile;
  StandTile *ptr_firstroom;
};
extern struct StanPrefixRecord *stan_prefix;
extern StandTile *stanTileEnd;
extern void stanBuildRoomData(void);
static struct StanPrefixRecord world_stan;
static u8 *world_tiles;
static StandTile *walk_tile;
static float walk_pos[3];
struct world_room {
  Vtx *v;
  Gfx *pri, *sec;
  unsigned pri_bytes,sec_bytes;
  struct NativeRoomPacketScope program;
  float pos[3];
  Mtx model;
};
static struct world_room world_rooms[137];
static u8 world_edges[256][4];
static unsigned world_edge_count;
static Gfx world_frame[2048];
static int world_loaded;
static Gfx *copy_list(unsigned *record,unsigned *written) {
  unsigned bytes;
  if (!record[0])
    return NULL;
  if (!convert_list(record[0], record[1], record[2], record[3], primary,
                    &bytes))
    return NULL;
  *written=bytes;
  Gfx *result = malloc(bytes);
  if (result)
    memcpy(result, primary, bytes);
  return result;
}
static int world_find_spawn(void) {
  coord3d pos = {4719.f, 100.f, 3949.f};
  float floor;
  walk_tile = stanFindTileBelowPos(&pos, NULL, &floor);
  if (!walk_tile || walk_tile->room != 135 || !__builtin_isfinite(floor))
    return 0;
  walk_pos[0] = pos.x;
  walk_pos[1] = floor;
  walk_pos[2] = pos.z;
  return 1;
}
void native_world_reset(void) {
  if (world_loaded) {
    world_find_spawn();
    native_motion_reset();
  }
}
unsigned native_world_room(void) { return walk_tile ? walk_tile->room : 0; }
void native_world_position(float *xyz) {
  memcpy(xyz, walk_pos, sizeof walk_pos);
}
int native_world_load(const char *path) {
  if (world_loaded)
    return 1;
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  u8 magic[8];
  unsigned h[5];
  int ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "EOGWORLD", 8) &&
           fread(h, 4, 5, f) == 5 && h[0] == 1 && h[1] == 136 && h[2] <= 256 &&
           h[3] == 41952;
  if (!ok) {
    fclose(f);
    return 0;
  }
  unsigned bytes = h[1] * 64 + h[2] * 4 + h[3];
  u8 *payload = malloc(bytes);
  ok = payload && fread(payload, 1, bytes, f) == bytes && fgetc(f) == EOF &&
       fnv(payload, bytes) == h[4];
  fclose(f);
  if (!ok) {
    free(payload);
    return 0;
  }
  world_edge_count = h[2];
  memcpy(world_edges, payload + h[1] * 64, h[2] * 4);
  world_tiles = calloc(1, 131072 + 128);
  if (!world_tiles) {
    free(payload);
    return 0;
  }
  world_tiles += 128;
  u8 *packed = payload + h[1] * 64 + h[2] * 4;
  if (packed[0] != 0x11 || packed[1] != 0x72 ||
      decompressdata(packed, world_tiles, inflate_tables) != 89040) {
    free(payload);
    return 0;
  }
  unsigned first = be32(world_tiles + 4), at = 4;
  while (at + 4 < first && be32(world_tiles + at))
    at += 4;
  if (at + 4 != first || first < 0x20 || first > 128) {
    free(payload);
    return 0;
  }
  at = first;
  static const unsigned sizes[11] = {32, 32, 32, 32, 40, 48,
                                     56, 64, 72, 80, 88};
  while (at + 8 <= 89040 && be32(world_tiles + at)) {
    u8 *tile = world_tiles + at;
    unsigned n = tile[6] >> 4;
    if (n < 3 || n > 10 || at + sizes[n] > 89040 || tile[3] >= 137) {
      free(payload);
      return 0;
    }
    u8 t = tile[0];
    tile[0] = tile[1];
    tile[1] = t;
    for (unsigned j = 4; j < 8 + n * 8; j += 2) {
      t = tile[j];
      tile[j] = tile[j + 1];
      tile[j + 1] = t;
    }
    stanTileEnd = (StandTile *)tile;
    at += sizes[n];
  }
  if (at + 4 > 89040 || be32(world_tiles + at)) {
    free(payload);
    return 0;
  }
  world_stan.stanfile = 0;
  world_stan.ptr_firstroom = (StandTile *)(world_tiles + first);
  stan_prefix = &world_stan;
  standTileStart = (StandTile *)(world_tiles + first - 0x80);
  setLevelScale(1.f);
  stanBuildRoomData();
  if (!world_find_spawn()) {
    free(payload);
    return 0;
  }
  // Reload in a shared pool; the isolated viewer is retained only in host
  // checks.
  g_TexCacheCount = 0;
  texInitPool(&textures, native_ram + 0x300000, 0x400000);
  for (unsigned i = 0; i < h[1]; ++i) {
    unsigned *r = (unsigned *)(payload + i * 64), id = r[0];
    if (id != i + 1 || id >= 137 || r[3] > MAX_RAW || r[3] % 16) {
      free(payload);
      return 0;
    }
    struct world_room *w = &world_rooms[id];
    memcpy(w->pos, r + 13, 12);
    if (!r[1])
      continue;
    w->v = malloc(r[3]);
    if (!w->v || !expand(r[1], r[2], r[3], r[4], (u8 *)w->v)) {
      free(payload);
      return 0;
    }
    for (unsigned a = 0; a < r[3]; a += 16)
      for (unsigned b = 0; b < 12; b += 2) {
        u8 *v = (u8 *)w->v + a + b, t = v[0];
        v[0] = v[1];
        v[1] = t;
      }
    header[6] = r[3];
    header[21] = 4094;
    w->pri = copy_list(r + 5,&w->pri_bytes);
    w->sec = copy_list(r + 9,&w->sec_bytes);
    if ((r[5] && !w->pri) || (r[9] && !w->sec)) {
      free(payload);
      return 0;
    }
  }
  free(payload);
  if (texFreeBytesInBuffer(&textures) < 8192)
    return 0;
  world_loaded = 1;
  sysLogPrintf(2,
               "Original Dam world: 136 rooms, %u portals, %u textures; spawn "
               "room %u floor %f",
               world_edge_count, (unsigned)(textures.end - textures.rightpos),
               native_world_room(), walk_pos[1]);
  return 1;
}
static int world_step(float x, float z) {
  StandTile *candidate = walk_tile;
  if (!candidate || !walkTilesBetweenPoints_NoCallback(&candidate, walk_pos[0],
                                                       walk_pos[2], x, z))
    return 0;
  float floor = stanGetPositionYValue(candidate, x, z);
  // Harness step limit; original Bond movement/prop collision is not wired yet.
  if (!__builtin_isfinite(floor) || floor - walk_pos[1] > 40.f ||
      walk_pos[1] - floor > 100.f)
    return 0;
  StandTile *edge = candidate;
  if (sub_GAME_7F0B20D0(&edge, x, z, 18.f) != STAN_COLLISION_NONE)
    return 0;
  walk_tile = candidate;
  walk_pos[0] = x;
  walk_pos[1] = floor;
  walk_pos[2] = z;
  return 1;
}
void native_world_move(float sideways, float forward, float yaw) {
  if (!world_loaded || !__builtin_isfinite(sideways) ||
      !__builtin_isfinite(forward) || !__builtin_isfinite(yaw))
    return;
  float dx = cosf(yaw) * sideways - sinf(yaw) * forward;
  float dz = -sinf(yaw) * sideways - cosf(yaw) * forward;
  float length = sqrtf(dx * dx + dz * dz);
  unsigned steps = (unsigned)(length / 4.f) + 1;
  if (steps > 64)
    return;
  dx /= steps;
  dz /= steps;
  for (unsigned i = 0; i < steps; i++) {
    if (!world_step(walk_pos[0] + dx, walk_pos[2] + dz)) {
      world_step(walk_pos[0] + dx, walk_pos[2]);
      world_step(walk_pos[0], walk_pos[2] + dz);
    }
  }
}
void native_world_update(float sideways, float forward, float yaw,
                         float seconds) {
  native_motion_tick(seconds, forward, sideways);
  float delta[3];
  native_motion_travel(delta);
  native_world_move(delta[0], delta[2], yaw);
}
static void normalise(float *v) {
  float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (length > 0.00001f)
    for (unsigned i = 0; i < 3; i++)
      v[i] /= length;
}
static void cross(const float *a, const float *b, float *r) {
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}
void native_world_render(float yaw, float pitch) {
  if (!world_loaded)
    return;
  unsigned char visible[137] = {0};
  visible[native_world_room()] = 1;
  for (unsigned depth = 0; depth < 4; depth++) {
    unsigned char next[137];
    memcpy(next, visible, sizeof next);
    for (unsigned i = 0; i < world_edge_count; i++) {
      unsigned a = world_edges[i][0], b = world_edges[i][1];
      if (a < 137 && b < 137 && (visible[a] || visible[b]))
        next[a] = next[b] = 1;
    }
    memcpy(visible, next, sizeof visible);
  }
  float c = cosf(yaw), s = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch),
        scale = .002f;
  float view[4][4] = {{c * scale, s * sp * scale, s * cp * scale, 0},
                      {0, cp * scale, -sp * scale, 0},
                      {-s * scale, c * sp * scale, c * cp * scale, 0},
                      {0, 0, 0, 1}};
  float head[3], look[3], up[3], back[3], world_up[3], right[3];
  native_motion_camera(head, look, up);
  for (unsigned i = 0; i < 3; i++) {
    back[i] =
        (view[i][0] * look[0] + view[i][1] * look[1] + view[i][2] * look[2]) /
        scale;
    world_up[i] =
        (view[i][0] * up[0] + view[i][1] * up[1] + view[i][2] * up[2]) / scale;
  }
  normalise(back);
  cross(world_up, back, right);
  normalise(right);
  cross(back, right, world_up);
  for (unsigned i = 0; i < 3; i++) {
    view[i][0] = right[i] * scale;
    view[i][1] = world_up[i] * scale;
    view[i][2] = back[i] * scale;
  }
  float eye[3] = {walk_pos[0], walk_pos[1] + head[1], walk_pos[2]};
  float f = 1.f / tanf(.6f), near = .02f, far = 40.f;
  float p[4][4] = {{f / (4.f / 3.f), 0, 0, 0},
                   {0, f, 0, 0},
                   {0, 0, (far + near) / (near - far), -1},
                   {0, 0, 2 * far * near / (near - far), 0}};
  pack_matrix(&projection, p);
  Gfx *g = world_frame;
  gDPPipeSync(g++);
  gSPViewport(g++, &viewport);
  gDPSetScissor(g++, G_SC_NON_INTERLACE, 0, 0, 440, 330);
  gSPClearGeometryMode(g++, 0xffffffff);
  gSPSetGeometryMode(g++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
  gDPSetTexturePersp(g++, G_TP_PERSP);
  gDPSetTextureFilter(g++, G_TF_POINT);
  gDPSetAlphaCompare(g++, G_AC_NONE);
  gSPMatrix(g++, &projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  // Match original pass ordering: all opaque rooms before translucent/decal
  // lists.
  for (unsigned pass = 0; pass < 2; pass++)
    for (unsigned id = 1; id < 137; id++) {
      struct world_room *w = &world_rooms[id];
      Gfx *list = pass ? w->sec : w->pri;
      if (!visible[id] || !list)
        continue;
      float v[4][4];
      memcpy(v, view, sizeof v);
      for (unsigned j = 0; j < 3; j++)
        v[3][j] = (w->pos[0] - eye[0]) * view[0][j] +
                  (w->pos[1] - eye[1]) * view[1][j] +
                  (w->pos[2] - eye[2]) * view[2][j];
      pack_matrix(&w->model, v);
      gSPMatrix(g++, &w->model, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
      gSPSegment(g++, 14, w->v);
      if(!pass) {
          native_scenery_scope(g++,1);
          w->program=(struct NativeRoomPacketScope){w->pri,w->pri_bytes,id,1};
          gDma0p(g++,NATIVE_ROOM_PACKET_COMMAND,&w->program,1);
      }
      gSPDisplayList(g++, list);
      if(!pass){gDma0p(g++,NATIVE_ROOM_PACKET_COMMAND,0,0);native_scenery_scope(g++,0);}
    }
  gSPEndDisplayList(g++);
  native_graphics_render(world_frame);
}
