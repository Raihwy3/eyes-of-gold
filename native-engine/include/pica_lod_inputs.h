#ifndef EOG_PICA_LOD_INPUTS_H
#define EOG_PICA_LOD_INPUTS_H
#include "pica_inputs.h"
#include "pica_combiner.h"
#include <cmath>
/* RSP-proven camera LOD input. Give it a stable ramp route even when every
 * vertex temporarily has the same value; otherwise moving the camera changes
 * the material's primary/constant/scalar interpretation and defeats reuse. */
inline bool pica_plan_lod_inputs(const float *data,unsigned stride,unsigned offset,unsigned count,
 unsigned inputs,bool alpha,unsigned rgb_hint,unsigned alpha_hint,PicaInputPlan *plan,unsigned *lod) {
 if(inputs<2 || inputs>3 || !rgb_hint || (rgb_hint&(rgb_hint-1)))return false;
 unsigned index=0;while((1u<<index)!=rgb_hint && index<3)index++;
 if(index>=inputs || (alpha_hint && alpha_hint!=rgb_hint))return false;
 unsigned channels=alpha?4:3,at=offset+index*channels;
 int scalar_alpha=-1;
 for(unsigned v=0;v<count;v++) {
   const float *p=data+v*stride+at;
   if(!std::isfinite(p[0]) || p[0]<0 || p[0]>1 || p[0]!=p[1] || p[0]!=p[2])return false;
   if(alpha) {
     if(alpha_hint){if(p[3]!=p[0])return false;}
     else {
       int a=p[3]==1.f?1:p[3]==0.f?0:-2;
       if(a<0 || (v && a!=scalar_alpha))return false;
       scalar_alpha=a;
     }
   }
 }
 PicaInputPlan result={0,-1,int(index),scalar_alpha,-1,-1};
 if(inputs==2)result.primary=1-index;
 else {
   for(unsigned other=0;other<inputs;other++)if(other!=index) {
     bool uniform=true;
     for(unsigned v=1;uniform && v<count;v++)for(unsigned c=0;c<channels;c++)
       if(std::fabs(data[v*stride+offset+other*channels+c]-data[offset+other*channels+c])>.00001f)uniform=false;
     if(uniform){result.constant=other;break;}
   }
   if(result.constant<0)return false;
   result.primary=3-index-result.constant;
 }
 *plan=result;*lod=at;return true;
}
/* Preflight the normalized route without changing the renderer's live plan.
 * Materials requiring incompatible TEV constants retain the original route. */
inline bool pica_lod_route_supported(const CCFeatures &cc,const PicaInputPlan &plan,
                                     const float *data,unsigned offset,bool alpha) {
 float constant[4]={1,1,1,1};
 if(plan.constant>=0)for(unsigned c=0;c<(alpha?4u:3u);c++)constant[c]=data[offset+plan.constant*(alpha?4:3)+c];
 for(unsigned cycle=0;cycle<(cc.opt_2cyc?2u:1u);cycle++)for(unsigned channel=0;channel<(alpha?2u:1u);channel++) {
   PicaEquation equation;if(!pica_plan_equation(cc,cycle,channel,&equation))return false;
   int required=-1;
   for(unsigned id:equation.ids) {
     int kind=-1;
     if(id==SHADER_0)kind=0;else if(id==SHADER_1)kind=1;
     else if(id>=SHADER_INPUT_1 && id<=SHADER_INPUT_3) {
       int slot=id-SHADER_INPUT_1;
       if(slot==plan.primary)continue;
       if(slot==plan.scalar){if(channel && plan.scalar_alpha>=0)kind=plan.scalar_alpha;}
       else if(slot==plan.constant)kind=2;else return false;
     }
     if(kind>=0) {
       kind=pica_constant_kind(kind,channel,constant);
       if(required>=0 && required!=kind)return false;
       required=kind;
     }
   }
 }
 return true;
}
#endif
