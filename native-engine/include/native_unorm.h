#ifndef NATIVE_UNORM_H
#define NATIVE_UNORM_H
/* Exact IEEE float results for byte / 255.f. A small read-only table avoids
 * repeating VFP division for each emitted material/fog colour component. */
#ifdef __cplusplus
#include <array>
static constexpr auto native_unorm_table=[] {
  std::array<float,256> values={};
  for(unsigned i=0;i<256;i++)values[i]=float(i)/255.f;
  return values;
}();
static inline float native_unorm8(unsigned value){return native_unorm_table[value];}
#endif
#endif
