#ifndef NATIVE_PICA_INPUTS_H
#define NATIVE_PICA_INPUTS_H
#include <stddef.h>
inline const float *pica_vertex_at(const float *data,size_t stride,size_t v,const float *const *refs) {return refs?refs[v]:data+v*stride;}
struct PicaInputPlan {
  int primary,constant,scalar,scalar_alpha;
  int primary_alpha,primary_fixed_alpha;
  bool zero_third=false;
};
/* Fast3D's color slots have different meanings for each material. Determine
 * Up to three inputs: determine which varying color goes to PICA's primary
 * color, which uniform input uses the TEV constant, and which scalar (LOD mix)
 * can use an auxiliary ramp texture. scalar_alpha: -1=same scalar, 0/1=fixed,
 * 2=primary alpha, 3=uniform alpha in TEV constant (constant==scalar,
 * RGB still uses the ramp). primary_alpha=-1 keeps the primary color's own alpha;
 * otherwise it selects another input's alpha and primary_fixed_alpha records
 * the displaced primary input's fixed alpha (0/1, or 2=constant input alpha).
 * In that last case constant==primary; its RGB still uses primary colour.
 */
bool pica_plan_inputs(const float *data,size_t stride,size_t offset,size_t count,
                      unsigned inputs,bool alpha,PicaInputPlan *plan);
bool pica_plan_inputs_view(const float *data,size_t stride,size_t offset,size_t count,
                      unsigned inputs,bool alpha,PicaInputPlan *plan,const float *const *refs);
#endif
