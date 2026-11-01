#ifndef NATIVE_PICA_FOG_H
#define NATIVE_PICA_FOG_H
#include "pica_inputs.h"
#include <math.h>
#include <stddef.h>
/* Fast3D puts fog RGBA between texture coords and material inputs. RGB is
 * constant per draw; A is the original RSP fog factor. PICA uses a gradient
 * texture and one final TEV interpolation stage, preserving material alpha. */
inline bool pica_plan_fog(const float *vertices, size_t stride, size_t offset,
                          size_t count, float *color,const float *const *refs=nullptr) {
  if (!count || offset + 4 > stride)
    return false;
  for (unsigned c = 0; c < 3; c++)
    color[c] = pica_vertex_at(vertices,stride,0,refs)[offset + c];
  for (size_t v = 0; v < count; v++) {
    const float *p = pica_vertex_at(vertices,stride,v,refs) + offset;
    for (unsigned c = 0; c < 4; c++)
      if (!__builtin_isfinite(p[c]))
        return false;
    for (unsigned c = 0; c < 3; c++)
      if (fabsf(p[c] - color[c]) > .00001f)
        return false;
  }
  return true;
}
inline float pica_ramp_coordinate(float factor) {
  if (factor < 0)
    factor = 0;
  if (factor > 1)
    factor = 1;
  return (factor * 255.f + .5f) / 256.f;
}
#endif
