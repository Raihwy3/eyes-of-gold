#ifndef EOG_NATIVE_CULL_H
#define EOG_NATIVE_CULL_H
#include <cmath>
inline unsigned native_cull_precise=0;
inline float native_viewmodel_facing(float ax,float ay,float aw,
                                     float bx,float by,float bw,
                                     float cx,float cy,float cw);
/* Sign-equivalent to the reference winding test after its negative-W fix.
 * Work in homogeneous coordinates: division by a corner's zero/tiny W must
 * not turn a whole foreground triangle into Inf/NaN and change its winding. */
inline float native_triangle_facing(float ax,float ay,float aw,
                                    float bx,float by,float bw,
                                    float cx,float cy,float cw) {
  float p0=cy*bw,p1=by*cw,p2=bx*cw,p3=cx*bw,p4=cx*by,p5=bx*cy;
  float result=ax*(p0-p1)+ay*(p2-p3)+aw*(p4-p5);
  float error=(std::fabs(ax)*(std::fabs(p0)+std::fabs(p1))+
               std::fabs(ay)*(std::fabs(p2)+std::fabs(p3))+
               std::fabs(aw)*(std::fabs(p4)+std::fabs(p5)))*1.e-6f;
  if(std::fabs(result)>error)return result;
  // The float sign can flip while moving the camera. Only uncertain
  // determinants use double precision; never reject on a cancelled sign.
  ++native_cull_precise;
  return native_viewmodel_facing(ax,ay,aw,bx,by,bw,cx,cy,cw);
}
/* Camera-anchored weapon facets can be tiny relative to homogeneous positions.
 * This is also the adaptive world path's fallback when a float sign is
 * uncertain. Return orientation, not magnitude. */
inline float native_viewmodel_facing(float ax,float ay,float aw,
                                     float bx,float by,float bw,
                                     float cx,float cy,float cw) {
    double result=double(ax)*(double(cy)*bw-double(by)*cw)
                 +double(ay)*(double(bx)*cw-double(cx)*bw)
                 +double(aw)*(double(cx)*by-double(bx)*cy);
    return result>0?1.f:result<0?-1.f:0.f;
}
#endif
