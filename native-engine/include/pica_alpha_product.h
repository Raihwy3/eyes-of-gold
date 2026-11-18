#ifndef NATIVE_PICA_ALPHA_PRODUCT_H
#define NATIVE_PICA_ALPHA_PRODUCT_H
#include "pica_combiner.h"
#include "pica_inputs.h"
#include <cmath>
/* This two-cycle material's final alpha is input0.A * input1.A.
 * If either factor is constant on each triangle, multiplying at vertices
 * preserves the exact perspective-interpolated product. This frees primary
 * alpha for the product while texture2 retains scalar RGB and original fog. */
inline bool pica_alpha_product_plan(const CCFeatures &c,const float *p,size_t stride,
                                    size_t offset,size_t count,PicaInputPlan *plan,const float *const *refs=nullptr) {
  if(!c.opt_alpha || !c.opt_2cyc || c.num_inputs!=2 || !count || count%3)return false;
  PicaEquation first,last;
  if(!pica_plan_equation(c,0,1,&first) || !pica_plan_equation(c,1,1,&last) ||
     first.kind!=PICA_REPLACE || first.ids[0]!=SHADER_INPUT_1 || last.kind!=PICA_MULTIPLY ||
     !((last.ids[0]==SHADER_COMBINED && last.ids[1]==SHADER_INPUT_2) ||
       (last.ids[1]==SHADER_COMBINED && last.ids[0]==SHADER_INPUT_2)))return false;
  for(size_t tri=0;tri<count;tri+=3) {
    bool uniform0=true,uniform1=true;
    for(size_t v=0;v<3;++v) {
      const float *a=pica_vertex_at(p,stride,tri+v,refs)+offset;
      if(!std::isfinite(a[0]) || std::fabs(a[0]-a[1])>.00001f || std::fabs(a[0]-a[2])>.00001f)return false;
      if(!std::isfinite(a[3]) || !std::isfinite(a[7]) || a[3]<0 || a[3]>1 || a[7]<0 || a[7]>1)return false;
      const float *base=pica_vertex_at(p,stride,tri,refs)+offset;
      uniform0 &= a[3]==base[3];uniform1 &= a[7]==base[7];
    }
    if(!uniform0 && !uniform1)return false;
  }
  *plan={1,-1,0,2,0,1};
  return true;
}
#endif
