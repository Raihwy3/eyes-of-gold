/* Original menu drawing and RLE decoder, driven by a small graphics harness.
 * This does not substitute a menu implementation for the original game loop.
 */
#include "native_graphics.h"
#include "native_ram.h"
#include <bondtypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
extern Gfx *titleRenderFolderMenuBackgroundLines(Gfx *, u8 *, s32,
                                                 struct FolderSelectColour *,
                                                 struct FolderSelectColour *);
extern void rle_expand_8bit(u8 *, u8 *);
static Gfx commands[4096];
static Mtx identity, rotation, projection;
static Vp viewport = {{{880, 660, 511, 0}, {880, 660, 511, 0}}};
static Vtx triangle[6];
static unsigned char *image;
static unsigned short be16(const unsigned char *p) {
  return p[0] * 256u + p[1];
}
int native_scene_load(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  static unsigned char encoded[262144];
  size_t bytes = fread(encoded, 1, sizeof encoded, f);
  int extra = fgetc(f);
  fclose(f);
  if (extra != EOF || bytes < 12 || be16(encoded) != 440 ||
      be16(encoded + 2) != 299)
    return 0;
  // Bound-check all runs before calling the original decoder, which trusts ROM.
  unsigned left = 440 * 299, pos = 10;
  while (left) {
    if (pos + 2 > bytes || !encoded[pos] || encoded[pos] > left)
      return 0;
    left -= encoded[pos];
    pos += 2;
  }
  if (!native_ram_init())
    return 0;
  image = native_ram + 0x10000;
  unsigned short w = 440, h = 299;
  memcpy(encoded, &w, 2);
  memcpy(encoded + 2, &h, 2);
  rle_expand_8bit(encoded, image);
  memset(&identity, 0, sizeof identity);
  // Fixed matrices pack two signed integer components per native u32 followed
  // by their two fractional halves, as consumed by the original interpreter.
  unsigned *m = (unsigned *)&identity;
  for (unsigned i = 0; i < 4; ++i)
    m[i * 2 + i / 2] |= (i & 1) ? 1 : 0x10000;
  return 1;
}
static Gfx *base(Gfx *p) {
  gDPPipeSync(p++);
  gDPSetCycleType(p++, G_CYC_1CYCLE);
  gDPSetScissor(p++, G_SC_NON_INTERLACE, 0, 0, 440, 330);
  gSPViewport(p++, &viewport);
  gDPSetTextureFilter(p++, G_TF_POINT);
  gDPSetTextureLOD(p++, G_TL_TILE);
  gDPSetTextureDetail(p++, G_TD_CLAMP);
  gDPSetTextureLUT(p++, G_TT_NONE);
  gDPSetAlphaCompare(p++, G_AC_NONE);
  gDPSetTexturePersp(p++, G_TP_NONE);
  gSPClearGeometryMode(p++, 0xffffffff);
  gDPSetRenderMode(p++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  return p;
}
void native_scene_frame(unsigned frame, unsigned mode) {
  Gfx *p = base(commands);
  if (!mode) {
    struct FolderSelectColour top = {0, 0, 0}, bottom = {255, 255, 255};
    gDPSetCombineMode(p++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    p = titleRenderFolderMenuBackgroundLines(
        p, image, (s32)(sinf(frame / 120.f) * 24), &top, &bottom);
  } else {
    // Controlled 3D probe: transformed colored triangles, perspective W and
    // overlapping depth, all through original GBI decoding/transform/batching.
    memcpy(&rotation, &identity, sizeof rotation);
    float a = frame / 100.f, c = cosf(a), s = sinf(a);
    float matrix[4][4] = {
        {c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    unsigned *m = (unsigned *)&rotation;
    memset(m, 0, sizeof rotation);
    for (unsigned i = 0; i < 4; ++i)
      for (unsigned j = 0; j < 4; ++j) {
        int fixed = (int)(matrix[i][j] * 65536);
        unsigned shift = (j & 1) ? 0 : 16;
        m[i * 2 + j / 2] |= ((unsigned)fixed >> 16 & 65535) << shift;
        m[8 + i * 2 + j / 2] |= ((unsigned)fixed & 65535) << shift;
      }
    static const short xy[6][2] = {{-1, -1}, {1, -1}, {0, 1},
                                   {-1, 0},  {1, 0},  {0, -1}};
    memset(triangle, 0, sizeof triangle);
    for (unsigned i = 0; i < 6; ++i) {
      triangle[i].v.ob[0] = xy[i][0];
      triangle[i].v.ob[1] = xy[i][1];
      triangle[i].v.ob[2] = i < 3 ? -1 : 1;
      triangle[i].v.cn[i % 3] = 255;
      triangle[i].v.cn[3] = 255;
    }
    gDPSetTexturePersp(p++, G_TP_PERSP);
    gSPSetGeometryMode(p++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetRenderMode(p++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gDPSetCombineMode(p++, G_CC_SHADE, G_CC_SHADE);
    if (mode == 3) {
      gSPSetGeometryMode(p++, G_FOG);
      gSPFogPosition(p++, 0, 1000);
      gDPSetFogColor(p++, 63, 95, 127, 0);
      gDPSetRenderMode(p++, G_RM_FOG_SHADE_A, G_RM_ZB_OPA_SURF2);
    }
    memset(&projection, 0, sizeof projection);
    unsigned *pm = (unsigned *)&projection;
    // x/y scale 0.5, z scale 0.25, perspective W = 1 + z/4.
    pm[8] = 0x80000000;
    pm[10] = 0x00008000;
    pm[13] = 0x40004000;
    pm[7] = 1;
    gSPMatrix(p++, &projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(p++, &rotation, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPVertex(p++, triangle, 6, 0);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSP1Triangle(p++, 3, 4, 5, 0);
  }
  gSPEndDisplayList(p++);
  native_graphics_render(commands);
}
void native_scene_destroy(void) { image = NULL; }
