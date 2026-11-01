#ifndef NATIVE_PICA_DEPTH_H
#define NATIVE_PICA_DEPTH_H
#include <stdint.h>
struct PicaDepthPlan {
  bool test, write, compare;
  float bias;
  bool inclusive=false;
};
/* The rooms use opaque, translucent (0x800) and coplanar decal passes. PICA's
 * reversed depth needs a positive raster bias for decals, with depth writes
 * disabled. This is a raster-depth approximation, not the RDP's variable
 * delta-Z coverage test. One D24 quantum was insufficient for independently
 * interpolated coplanar surfaces; keep the offset limited to decal mode. */
inline bool pica_plan_depth(bool test, bool write, bool compare, bool primitive,
                            uint16_t mode, PicaDepthPlan *plan) {
  if (primitive || (mode != 0 && mode != 0x400 && mode != 0x800 && mode != 0xc00))
    return false;
  *plan = {test, test && write && mode != 0xc00, compare,
           mode == 0xc00 ? 4.f / 1048576.f : 0.f, mode == 0xc00 || mode == 0x400};
  return true;
}
#endif
