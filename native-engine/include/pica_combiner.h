#ifndef NATIVE_PICA_COMBINER_H
#define NATIVE_PICA_COMBINER_H
#include "gfx_cc.h"
enum PicaEquationKind { PICA_REPLACE, PICA_MULTIPLY, PICA_MIX, PICA_MULTIPLY_ADD, PICA_ADD };
/* TEV has one constant register per stage, but a uniform input whose
 * channel is exactly zero/one shares that literal's register value. RGB and
 * alpha are independent portions of the same packed register. */
inline unsigned pica_constant_kind(unsigned kind,unsigned channel,const float rgba[4]) {
  if(kind!=2)return kind;
  if(channel) {
    if(rgba[3]==0.f)return 0;
    if(rgba[3]==1.f)return 1;
  } else {
    if(rgba[0]==0.f && rgba[1]==0.f && rgba[2]==0.f)return 0;
    if(rgba[0]==1.f && rgba[1]==1.f && rgba[2]==1.f)return 1;
  }
  return 2;
}
struct PicaEquation { PicaEquationKind kind; unsigned ids[3]; };
inline bool pica_plan_equation(const CCFeatures &c, unsigned cycle,
                               unsigned channel, PicaEquation *out) {
  const auto *terms=c.c[cycle][channel];
  if (c.do_single[cycle][channel])
    *out={PICA_REPLACE,{terms[3],terms[3],terms[3]}};
  else if (c.do_multiply[cycle][channel]) {
    // Literal identity factors must not consume a second TEV constant slot.
    if (terms[0]==SHADER_1)
      *out={PICA_REPLACE,{terms[2],terms[2],terms[2]}};
    else if (terms[2]==SHADER_1)
      *out={PICA_REPLACE,{terms[0],terms[0],terms[0]}};
    else if (terms[0]==SHADER_0)
      *out={PICA_REPLACE,{SHADER_0,SHADER_0,SHADER_0}};
    else *out={PICA_MULTIPLY,{terms[0],terms[2],terms[2]}};
  }
  else if (c.do_mix[cycle][channel])
    *out={PICA_MIX,{terms[0],terms[1],terms[2]}};
  else if (terms[1]==SHADER_0) {
    // (1-0)*B+D and (A-0)*1+D need only ADD, avoiding an
    // unnecessary literal-one constant alongside a uniform input.
    if(terms[0]==SHADER_1)
      *out={PICA_ADD,{terms[2],terms[3],terms[3]}};
    else if(terms[2]==SHADER_1)
      *out={PICA_ADD,{terms[0],terms[3],terms[3]}};
    else *out={PICA_MULTIPLY_ADD,{terms[0],terms[2],terms[3]}};
  }
  else return false;
  return true;
}
#endif
