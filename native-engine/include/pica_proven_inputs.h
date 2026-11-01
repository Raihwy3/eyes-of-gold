#ifndef EOG_PICA_PROVEN_INPUTS_H
#define EOG_PICA_PROVEN_INPUTS_H
#include "pica_lod_inputs.h"
/* Producer proofs apply to the whole pending batch, not merely its first
 * triangle. No per-vertex value scans are needed in these conservative cases. */
inline bool pica_proven_inputs(const CCFeatures &cc,const float *data,unsigned offset,
 unsigned uniform,unsigned alpha_uniform,unsigned lod_rgb,unsigned lod_alpha,PicaInputPlan *out) {
 unsigned n=cc.num_inputs,channels=cc.opt_alpha?4:3;
 PicaInputPlan plan={0,-1,-1,-1,-1,-1};
 if(n<2){*out=plan;return true;}
 if(n==2 && (uniform&3)) {
   plan.constant=uniform&2?1:0;plan.primary=1-plan.constant;
   // This is exactly the original planner's preferred uniform-input route.
   *out=plan;return true;
 }
 if(cc.opt_noise)return false; // Keep spare texture-unit allocation for screen noise.
 if(n>3 || !lod_rgb || (lod_rgb&(lod_rgb-1)) || (lod_alpha && lod_alpha!=lod_rgb))return false;
 unsigned lod=0;while(lod<3 && (1u<<lod)!=lod_rgb)lod++;
 if(lod>=n)return false;
 plan.scalar=lod;
 if(cc.opt_alpha && !lod_alpha) {
   if(!(alpha_uniform&(1u<<lod)))return false;
   float a=data[offset+lod*channels+3];
   if(a!=0 && a!=1)return false;
   plan.scalar_alpha=a==1?1:0;
 }
 if(n==2)plan.primary=1-lod;
 else {
   for(unsigned i=0;i<n;i++)if(i!=lod && (uniform&(1u<<i))){plan.constant=i;break;}
   if(plan.constant<0)return false;
   plan.primary=3-lod-plan.constant;
 }
 if(!pica_lod_route_supported(cc,plan,data,offset,cc.opt_alpha))return false;
 *out=plan;return true;
}
#endif
