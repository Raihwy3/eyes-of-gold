/* Original gait model, original animation decode and original bhead filters. */
#include "native_motion.h"
#include "game/bondhead.h"
#include "game/bondview.h"
#include "game/chrobjdata.h"
#include "game/initBondDATAdefaults.h"
#include "game/initanitable.h"
#include "game/lv.h"
#include "game/model.h"
#include "game/player.h"
#include "native_cart.h"
#include "romdata.h"
#include <stdio.h>
#include <string.h>
/* ARM links original gobjdata.o. Host checks compile just these original
 * declarations/definitions, retaining their dependency order. */
#ifndef __3DS__
// clang-format off
#include "assets/embedded/skeletons/guard.inc.c"
#include "assets/embedded/skeletons/player_gait_object.inc.c"
#include "assets/embedded/player_gait_object/modelnode.inc.c"
#include "assets/embedded/player_gait_object/header.inc.c"
// clang-format on
#endif
extern void expand_ani_table_entries(s32 **);
extern void initAnimationsBuffer(struct bondstruct_unk_animation_related *,
                                 OSMesgQueue *, s32);
extern struct bondstruct_unk_animation_related D_80029D60;
extern void init_player_gait_object(void);
unsigned char
    *g_pc_animdata_base; /* Native animation symbols are offsets from zero. */
static struct player native_player;
static unsigned char table[0xe7e0] __attribute__((aligned(16)));
static int ready;
static float ticks, travel[3];
static unsigned hash(const unsigned char *p, unsigned n) {
  unsigned h = 2166136261u;
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}
void native_motion_reset(void) {
  if (!ready)
    return;
  memset(&native_player, 0, sizeof native_player);
  g_CurrentPlayer = &native_player;
  ticks = 0;
  memset(travel, 0, sizeof travel);
  modelResetAnimationsScratchBuffer();
  sets_a_bunch_of_BONDdata_values_to_default();
  g_ClockTimer = 1;
  g_GlobalTimerDelta = 1.f;
  bheadAdjustAnimation(0);
  bheadUpdate(0, 0);
}
int native_motion_load(const char *path) {
  if (ready)
    return 1;
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  char magic[8];
  unsigned h[6];
  int ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "EOGGAIT1", 8) &&
           fread(h, 4, 6, f) == 6 && h[0] == 1 && h[1] == 0x169ec0 &&
           h[2] == sizeof table && h[5] == 0;
  unsigned char block[4096];
  unsigned digest = 2166136261u;
  if (ok)
    for (unsigned left = h[1]; left;) {
      unsigned n = left > sizeof block ? sizeof block : left;
      if (fread(block, 1, n, f) != n) {
        ok = 0;
        break;
      }
      for (unsigned i = 0; i < n; i++)
        digest = (digest ^ block[i]) * 16777619u;
      left -= n;
    }
  ok = ok && digest == h[3] &&
       fread(table, 1, sizeof table, f) == sizeof table &&
       hash(table, sizeof table) == h[4] && fgetc(f) == EOF;
  fclose(f);
  if (!ok)
    return 0;
  if (!native_cart_mount(path, 0x10124ac0, 32, h[1]))
    return 0;
  ptr_animation_table = (struct animation_table_data *)table;
  romdataFixupAnimationData(table, sizeof table, animation_table_ptrs1,
                            animation_table_ptrs2);
  expand_ani_table_entries((s32 **)animation_table_ptrs1);
  expand_ani_table_entries((s32 **)animation_table_ptrs2);
  initAnimationsBuffer(&D_80029D60, NULL, 0);
  init_player_gait_object();
  ready = 1;
  native_motion_reset();
  return 1;
}
void native_motion_tick(float seconds, float forward, float sideways) {
  memset(travel, 0, sizeof travel);
  if (!ready || !__builtin_isfinite(seconds) || seconds <= 0 || seconds > .1f ||
      !__builtin_isfinite(forward) || !__builtin_isfinite(sideways))
    return;
  ticks += seconds * 60.f;
  while (ticks >= 1.f) {
    ticks -= 1.f;
    g_ClockTimer = 1;
    g_GlobalTimerDelta = 1.f;
    modelResetAnimationsScratchBuffer();
    float f = forward < 0 ? -forward : forward,
          s = sideways < 0 ? -sideways : sideways;
    float speed = f > s ? f : s;
    bheadAdjustAnimation(speed);
    bheadUpdate(speed > 0 ? forward / speed : 0,
                sideways * g_BondMoveAnimationSetup[1].speedMultiplier * .5f);
    travel[0] += native_player.headpos.f[0];
    travel[2] += native_player.headpos.f[2];
  }
}
void native_motion_travel(float *xyz) { memcpy(xyz, travel, sizeof travel); }
void native_motion_camera(float *position, float *look, float *up) {
  float p[3] = {0, 160, 0}, l[3] = {0, 0, 1}, u[3] = {0, 1, 0};
  if (ready) {
    p[1] = native_player.headpos.f[1];
    memcpy(l, &native_player.headlook, 12);
    memcpy(u, &native_player.headup, 12);
  }
  memcpy(position, p, 12);
  memcpy(look, l, 12);
  memcpy(up, u, 12);
}
