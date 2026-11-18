#ifndef NATIVE_PICA_TEV_CACHE_H
#define NATIVE_PICA_TEV_CACHE_H
#include "pica_inputs.h"
#include <stdint.h>
#include <stddef.h>
struct PicaTevKey {
  uint64_t shader;uint32_t flags;PicaInputPlan inputs;unsigned rgb_kind,alpha_kind;
};
inline bool pica_tev_same(const PicaTevKey &a,const PicaTevKey &b) {
  return a.shader==b.shader && a.flags==b.flags && a.rgb_kind==b.rgb_kind && a.alpha_kind==b.alpha_kind &&
    a.inputs.zero_third==b.inputs.zero_third && a.inputs.primary==b.inputs.primary && a.inputs.constant==b.inputs.constant &&
    a.inputs.scalar==b.inputs.scalar && a.inputs.scalar_alpha==b.inputs.scalar_alpha &&
    a.inputs.primary_alpha==b.inputs.primary_alpha && a.inputs.primary_fixed_alpha==b.inputs.primary_fixed_alpha;
}
/* Direct-mapped, bounded descriptor cache: no allocations or linear search.
 * Collisions replace a descriptor, never reuse a non-matching material. */
inline uint32_t pica_tev_hash(const PicaTevKey &k) {
  uint32_t h=uint32_t(k.shader)^uint32_t(k.shader>>32);
  auto mix=[&](uint32_t v){h=(h^v)*16777619u;};
  mix(k.flags);mix(k.inputs.primary);mix(k.inputs.constant);mix(k.inputs.scalar);
  mix(k.inputs.scalar_alpha);mix(k.inputs.primary_alpha);mix(k.inputs.primary_fixed_alpha);
  if(k.inputs.zero_third)mix(1);
  mix(k.rgb_kind);mix(k.alpha_kind);return h;
}
template<class Value,unsigned Capacity=128> class PicaTevCache {
  static_assert(Capacity && !(Capacity&(Capacity-1)));
  struct Entry {bool valid=false;PicaTevKey key={};Value value={};};
  Entry entries[Capacity];
public:
  Value *find(const PicaTevKey &key) {
    Entry &e=entries[pica_tev_hash(key)&(Capacity-1)];
    return e.valid && pica_tev_same(e.key,key)?&e.value:nullptr;
  }
  void put(const PicaTevKey &key,const Value &value) {
    Entry &e=entries[pica_tev_hash(key)&(Capacity-1)];e.key=key;e.value=value;e.valid=true;
  }
};
struct PicaDrawState {
  PicaTevKey tev;
  uint32_t colours[3],textures[2];
};
inline bool pica_draw_same(const PicaDrawState &a,const PicaDrawState &b) {
  return pica_tev_same(a.tev,b.tev) && a.colours[0]==b.colours[0] &&
    a.colours[1]==b.colours[1] && a.colours[2]==b.colours[2] &&
    a.textures[0]==b.textures[0] && a.textures[1]==b.textures[1];
}
struct PicaOrderedDraw {
  PicaDrawState state={};size_t first=0,count=0;
  bool extendable(const PicaDrawState &next,size_t next_first) const {
    return count && first+count==next_first && pica_draw_same(state,next);
  }
  void begin(const PicaDrawState &next,size_t start,size_t length){state=next;first=start;count=length;}
  void clear(){count=0;}
};
#endif
