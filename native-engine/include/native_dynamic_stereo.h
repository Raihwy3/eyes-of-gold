#pragma once
#include "native_stereo_profile.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../vendor/rt64-stereo/stereo_math.hpp"
/* Clip-space stereo using the rt64-3ds convergence/statistic implementation
 * and its candidate-strip reticle sampler (see docs/gyro-stereo.md).
 * Distances below are homogeneous camera W, not device-depth values. */
namespace native_stereo {
constexpr float separation=native_stereo_profile::separation, manual_convergence=native_stereo_profile::manual_convergence;
inline void small_sort(float *a,unsigned n){
 for(unsigned i=1;i<n;i++){float v=a[i];unsigned j=i;while(j && a[j-1]>v){a[j]=a[j-1];--j;}a[j]=v;}
}
inline float disparity(float distance,float convergence,float slider){
 if(!std::isfinite(slider) || slider<=0)return 0;
 return std::clamp(slider,0.f,2.f)*separation*(1.f-(distance>0 && std::isfinite(distance)?convergence/distance:0.f));
}
struct Projection {
 float ratio=0,b=0;bool valid=false;
 static Projection from(float p22,float p23,float p32,float p33){
  Projection p;
  if(std::isfinite(p22) && std::isfinite(p23) && std::isfinite(p32) &&
     std::isfinite(p33) && p23<-.000001f && std::fabs(p33)<.000001f && p32<0){
   p.ratio=p22/p23;p.b=p32;p.valid=true;
  }
  return p;
 }
 float canonical(uint32_t packed,float scale,float bias)const{
  uint32_t d=packed&0xffffffu;
  if(!valid || !d || d==0xffffffu || !std::isfinite(scale) || std::fabs(scale)<1.e-6f)return 0;
  float zp=(float(d)/16777215.f-bias)/scale+1.f;
  return std::isfinite(zp) && zp>0 && zp<1?zp:0;
 }
 float distance_from_canonical(float zp)const{
  if(!valid || !(zp>0 && zp<1))return 0;
  float w=b/(2*zp-1-ratio);
  return std::isfinite(w) && w>0 && w<1.e7f?w:0;
 }
 float near_plane()const{return valid?b/(-1.f-ratio):4.f;}
 float distance(uint32_t packed,float scale,float bias)const{return distance_from_canonical(canonical(packed,scale,bias));}

};
inline unsigned tiled_index(unsigned x,unsigned y,unsigned width){
 unsigned m=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
 return ((y>>3)*(width>>3)+(x>>3))*64+m;
}

struct Convergence {
 eog_rt64_stereo::AutoConvergence reference;
 float value=manual_convergence;
 void reset(){reference.reset();value=manual_convergence;}
 float update(float near,float slider,float seconds,float near_plane=4.f){
  eog_rt64_stereo::AutoConvTuning tune;
  // Reference rates at 60 Hz; preserve their time constants at 30 Hz.
  float ticks=std::clamp(seconds*60.f,.25f,6.f);
  auto rate=[&](float a){return 1.f-std::pow(1.f-a,ticks);};
  tune.approach=rate(tune.approach);tune.retreat=rate(tune.retreat);
  tune.ease_in=rate(tune.ease_in);tune.ease_out=rate(tune.ease_out);
  tune.near_clamp=std::max(4.f,near_plane);
  if(!std::isfinite(near))near=0;
  // Reference default: nearest significant geometry stays at the LCD plane.
  value=reference.update(near,manual_convergence,separation*std::clamp(slider,0.f,2.f),0,tune);
  return value;
 }
};
/* Full 8-pixel grid. Same 9x5 patches, 25th percentile and second-patch
 * rejection as rt64-3ds; samples are canonical [0,1] projection depth. */
template<class Read> float near_depth(Read read,int width=400,int height=240){
 float grid[50*30];
 int cols=std::clamp((width+7)/8,1,50),rows=std::clamp((height+7)/8,1,30);
 for(int y=0;y<rows;y++)for(int x=0;x<cols;x++)
  grid[y*cols+x]=read(std::min(x*8+4,width-1)/float(width),std::min(y*8+4,height-1)/float(height));
 return eog_rt64_stereo::near_depth_statistic(grid,cols,rows,.75f);
}
struct AimDepth {
 bool have=false;float value=-1;
 void reset(){have=false;value=-1;}
 // `read` returns view/camera W at actual visible screen pixels, 0 for sky.
 template<class Read> float sample(Read read,float cx,float cy,float width,float slider,float conv,float seconds){
  constexpr int candidates=13,patch=5,min_samples=2;
  float s=separation*slider*width*.5f;
  bool shifted=s>.5f && conv>0;
  float lo=shifted?-s:0,hi=shifted?2*s:0,spacing=(hi-lo)/(candidates-1);
  float tolerance=std::max(spacing,patch*.5f),aim_inv=-1,best_res=1.e9f,best_inv=-1;
  for(int i=0;i<(shifted?candidates:1);i++){
   float offset=lo+spacing*i;float z[25];unsigned n=0;
   for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
    float d=read(std::round(cx)+std::round(offset)+dx,std::round(cy)+dy);
    if(d>0 && std::isfinite(d))z[n++]=d;
   }
   float inv=0,predicted=lo;
   if(n>=min_samples){std::nth_element(z,z+n/2,z+n);inv=1.f/z[n/2];predicted=shifted?s*(conv*inv-1.f):0;}
   float residual=std::fabs(predicted-offset);
   if(residual<=tolerance){if(inv>aim_inv)aim_inv=inv;}
   else if(residual<best_res){best_res=residual;best_inv=inv;}
  }
  if(aim_inv<0 && !have)aim_inv=best_inv;
  if(aim_inv>0){
   float z=1.f/aim_inv;
   if(!have)value=z;
   else if(std::fabs(z-value)/value>.01f)value+=(z-value)*(1.f-std::pow(.75f,std::clamp(seconds*60.f,.25f,6.f)));
   have=true;
  }
  return have?value:-1;
 }
 float shift(float conv,float slider)const{
  float d=have?disparity(value,conv,slider):0;
  // Reference NDC limits: +0.10 behind, -0.30 in front.
  return std::clamp(d,-.30f,.10f);
 }
};
}
