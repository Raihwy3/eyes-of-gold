#pragma once
/* PICA clip Z lies in [-W,0]. Its -Z/W term provides inverse-distance
 * disparity without changing depth, UVs, geometry or the world projection. */
template<typename Matrix>
inline void native_depth_stereo(Matrix &out,const Matrix &flat,float slider,bool right,float base_pixels,float depth_pixels){
 if(!(slider>0.f))slider=0.f;
 if(slider>1.f)slider=1.f;
 out=flat;
 const float direction=right?-1.f:1.f;
 const float base=direction*slider*base_pixels/200.f;
 const float depth=direction*slider*depth_pixels/200.f;
 for(unsigned row=0;row<4;row++){
  out.r[row].z-=flat.r[row].x*depth;
  out.r[row].w+=flat.r[row].x*base;
 }
}
template<typename Matrix>
inline void native_weapon_stereo(Matrix &out,const Matrix &flat,float slider,bool right){
 native_depth_stereo(out,flat,slider,right,1.f,4.f);
}
template<typename Matrix>
inline void native_world_stereo(Matrix &out,const Matrix &flat,float slider,bool right){
 // Distant scenery sits behind the LCD; nearby geometry crosses its plane.
 // Leave depth and W untouched, including CPU-clipped and GPU palette draws.
 native_depth_stereo(out,flat,slider,right,-2.f,12.f);
}
