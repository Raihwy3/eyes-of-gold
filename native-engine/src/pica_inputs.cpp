#include "pica_inputs.h"
#include <cmath>
static bool near(float a, float b) { return std::fabs(a - b) < .00001f; }
bool pica_plan_inputs_view(const float *data, size_t stride, size_t offset,
                      size_t count, unsigned inputs, bool alpha,
                      PicaInputPlan *plan,const float *const *refs) {
  *plan = {0, -1, -1, -1, -1, -1};
  if (inputs < 2)
    return true;
  if (inputs > 3 || !count)
    return false;
  unsigned channels = alpha ? 4 : 3;
  bool uniform[3] = {true, true, true};
  /* Prefer the common second constant immediately. The old code scanned
   * input 0 as well even when that entire result would be discarded. This
   * also avoids a second full walk for flat-shaded/UI batches. */
  for (unsigned pass = 0; pass < inputs; ++pass) {
    unsigned input=inputs==2?1-pass:pass;
    const float *first=pica_vertex_at(data,stride,0,refs)+offset+input*channels;
    for (size_t v = 1; v < count && uniform[input]; ++v) {
      const float *p=pica_vertex_at(data,stride,v,refs)+offset+input*channels;
      for (unsigned c = 0; c < channels; ++c)
        if (!near(first[c],p[c])) {uniform[input]=false;break;}
    }
    if(inputs==2 && uniform[input]) {
      plan->constant=input;plan->primary=1-input;return true;
    }
  }
  PicaInputPlan fallback={0,-1,-1,-1,-1,-1};
  bool has_fallback=false;
  for (unsigned input = 0; input < inputs; ++input) {
    bool scalar = true, same_alpha = true, zero_alpha = true, one_alpha = true, uniform_alpha = true;
    float first_alpha=alpha?pica_vertex_at(data,stride,0,refs)[offset+input*channels+3]:0.f;
    for (size_t v = 0; v < count; ++v) {
      const float *p = pica_vertex_at(data,stride,v,refs) + offset + input * channels;
      if(!near(p[0],p[1]) || !near(p[1],p[2])) {scalar=false;break;}
      if (alpha) {
        uniform_alpha &= near(p[3], first_alpha);
        same_alpha &= near(p[0], p[3]);
        zero_alpha &= near(p[3], 0);
        one_alpha &= near(p[3], 1);
      }
    }
    if (scalar) {
      PicaInputPlan candidate={0,-1,-1,-1,-1,-1};
      if (inputs == 3) {
        int constant = -1;
        for (unsigned other = 0; other < inputs; ++other)
          if (other != input && uniform[other]) { constant = other; break; }
        if (constant < 0) continue;
        candidate.constant = constant;
        candidate.primary = 3 - input - constant;
      } else candidate.primary = 1 - input;
      candidate.scalar = input;
      candidate.scalar_alpha = !alpha || same_alpha ? -1 : one_alpha ? 1 : 0;
      if(alpha && !same_alpha && !zero_alpha && !one_alpha) {
        bool primary_zero=true,primary_one=true,primary_uniform=true;
        float first_primary_alpha=pica_vertex_at(data,stride,0,refs)[offset+candidate.primary*channels+3];
        for(size_t v=0;v<count;++v) {
          float a=pica_vertex_at(data,stride,v,refs)[offset+candidate.primary*channels+3];
          primary_uniform &= near(a,first_primary_alpha);
          primary_zero &= near(a,0.f); primary_one &= near(a,1.f);
        }
        if(!primary_zero && !primary_one) {
          if(inputs==2 && uniform_alpha) {
            candidate.constant=input; // TEV constant alpha only; RGB stays on ramp.
            candidate.scalar_alpha=3;
            fallback=candidate;has_fallback=true;
          }
          else if(inputs==2 && primary_uniform) {
            // Primary RGB varies, but its uniform alpha can use the TEV
            // constant while primary alpha carries the scalar's varying A.
            candidate.constant=candidate.primary;
            candidate.primary_alpha=input;
            candidate.primary_fixed_alpha=2;
            candidate.scalar_alpha=2;
            fallback=candidate;has_fallback=true;
          }
          continue;
        }
        candidate.primary_alpha=input;
        candidate.primary_fixed_alpha=primary_one?1:0;
        candidate.scalar_alpha=2;
      }
      *plan=candidate;
      return true;
    }
  }
  if(has_fallback) {*plan=fallback;return true;}
  if(inputs==3 && alpha) {
    bool zero=true;
    for(size_t v=0;v<count && zero;v++)for(unsigned c=0;c<4;c++)
      if(pica_vertex_at(data,stride,v,refs)[offset+8+c]!=0.f){zero=false;break;}
    if(zero && pica_plan_inputs_view(data,stride,offset,count,2,true,plan,refs)) {
      plan->zero_third=true;return true;
    }
  }
  return false;
}

bool pica_plan_inputs(const float *data,size_t stride,size_t offset,size_t count,unsigned inputs,bool alpha,PicaInputPlan *plan) {
 return pica_plan_inputs_view(data,stride,offset,count,inputs,alpha,plan,nullptr);
}
