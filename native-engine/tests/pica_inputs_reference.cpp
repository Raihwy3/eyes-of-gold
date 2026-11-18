/* Frozen pre-optimization planner, for equivalence checks only. */
#include "pica_inputs.h"
#include <cmath>
static bool near(float a, float b) { return std::fabs(a - b) < .00001f; }
bool pica_plan_inputs_reference(const float *data, size_t stride, size_t offset,
                      size_t count, unsigned inputs, bool alpha,
                      PicaInputPlan *plan) {
  *plan = {0, -1, -1, -1, -1, -1};
  if (inputs < 2)
    return true;
  if (inputs > 3 || !count)
    return false;
  unsigned channels = alpha ? 4 : 3;
  bool uniform[3] = {true, true, true};
  for (unsigned input = 0; input < inputs; ++input) {
    const float *first=data+offset+input*channels;
    for (size_t v = 1; v < count && uniform[input]; ++v) {
      const float *p=data+v*stride+offset+input*channels;
      for (unsigned c = 0; c < channels; ++c)
        if (!near(first[c],p[c])) {uniform[input]=false;break;}
    }
  }
  if (inputs == 2 && uniform[1]) {
    plan->constant = 1;
    return true;
  }
  if (inputs == 2 && uniform[0]) {
    plan->primary = 1;
    plan->constant = 0;
    return true;
  }
  for (unsigned input = 0; input < inputs; ++input) {
    bool scalar = true, same_alpha = true, zero_alpha = true, one_alpha = true;
    for (size_t v = 0; v < count; ++v) {
      const float *p = data + v * stride + offset + input * channels;
      if(!near(p[0],p[1]) || !near(p[1],p[2])) {scalar=false;break;}
      if (alpha) {
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
        bool primary_zero=true,primary_one=true;
        for(size_t v=0;v<count;++v) {
          float a=data[v*stride+offset+candidate.primary*channels+3];
          primary_zero &= near(a,0.f); primary_one &= near(a,1.f);
        }
        if(!primary_zero && !primary_one) continue;
        candidate.primary_alpha=input;
        candidate.primary_fixed_alpha=primary_one?1:0;
        candidate.scalar_alpha=2;
      }
      *plan=candidate;
      return true;
    }
  }
  return false;
}
