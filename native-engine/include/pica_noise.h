#ifndef NATIVE_PICA_NOISE_H
#define NATIVE_PICA_NOISE_H
#include "gfx_cc.h"
#include <cstdint>
inline uint32_t pica_noise_hash(unsigned x,unsigned y,unsigned frame) {
  uint32_t n=x*0x9e3779b9u ^ y*0x85ebca6bu ^ frame*0xc2b2ae35u;
  n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;
  return n;
}
// Noise is applied after the entire material, including texture alpha. Keep
// the material alpha in the TEV buffer while approximating a step with steep
// saturated subtract/scale stages. No dependent texture reads are required.
struct PicaNoisePlan { int texture, first, scales, last; };
inline bool pica_plan_noise(const CCFeatures &c,bool scalar,PicaNoisePlan *out) {
  if(!c.opt_noise || !c.opt_alpha) return false;
  int slot=!c.used_textures[0]?0:!c.used_textures[1]?1:!scalar&&!c.opt_fog?2:-1;
  if(slot<0) return false;
  int first=(c.opt_2cyc?2:1)+(c.opt_fog?1:0);
  int scales=first<=2?3:2;
  *out={slot,first,scales,first+scales};
  return out->last<6;
}
inline unsigned char pica_noise_sample(unsigned x,unsigned y,int scales) {
  unsigned max=scales==3?251:239;
  return (pica_noise_hash(x,y,0)>>24)*max/255;
}
#endif
