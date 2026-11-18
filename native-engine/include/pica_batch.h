#ifndef NATIVE_PICA_BATCH_H
#define NATIVE_PICA_BATCH_H
#include "pica_inputs.h"
#include "pica_fog.h"
/* A Fast3D batch may contain triangles with different uniform inputs. Keep
 * the longest compatible prefix; never average colors or remove triangles. */
inline bool pica_batch_fits(const float *p,size_t stride,size_t offset,
                            size_t triangles,unsigned inputs,bool alpha,bool fog,
                            PicaInputPlan *out_plan=nullptr,float *out_rgb=nullptr,const float *const *refs=nullptr) {
  PicaInputPlan plan;float rgb[3];
  if(!out_plan)out_plan=&plan;
  if(!out_rgb)out_rgb=rgb;
  return pica_plan_inputs_view(p,stride,offset,triangles*3,inputs,alpha,out_plan,refs) &&
    (!fog || pica_plan_fog(p,stride,offset-4,triangles*3,out_rgb,refs));
}
inline size_t pica_batch_prefix(const float *p,size_t stride,size_t offset,
                                size_t triangles,unsigned inputs,bool alpha,bool fog,const float *const *refs=nullptr) {
  if(!triangles || !pica_batch_fits(p,stride,offset,1,inputs,alpha,fog,nullptr,nullptr,refs))return 0;
  // Most failed batches have a short compatible run. Find its scale before
  // bisecting, instead of repeatedly probing half the entire remaining draw.
  size_t low=1,high=2;
  while(high<=triangles && pica_batch_fits(p,stride,offset,high,inputs,alpha,fog,nullptr,nullptr,refs)){
    low=high;
    if(high>triangles/2){high=triangles+1;break;}
    high*=2;
  }
  if(high>triangles)high=triangles+1;
  while(low+1<high) {
    size_t mid=low+(high-low)/2;
    if(pica_batch_fits(p,stride,offset,mid,inputs,alpha,fog,nullptr,nullptr,refs))low=mid;
    else high=mid;
  }
  return low;
}
#endif
