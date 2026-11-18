#pragma once
#include "native_stereo_profile.h"
/* Eye convergence shifts clip X by a W term. The shaders add the eye
 * translation separately, before perspective division, using camera-space W. */
template<typename Matrix>
inline void native_depth_stereo(Matrix &out,const Matrix &flat,float slider,bool right,float base_pixels){
 if(!(slider>0.f))slider=0.f;
 if(slider>2.f)slider=2.f;
 out=flat;
 const float direction=right?-1.f:1.f;
 const float base=direction*slider*base_pixels/200.f;
 for(unsigned row=0;row<4;row++){
  out.r[row].w+=flat.r[row].x*base;
 }
}
template<typename Matrix>
inline void native_weapon_stereo(Matrix &out,const Matrix &flat,float slider,bool right){
 native_depth_stereo(out,flat,slider,right,1.f);
}
template<typename Matrix>
inline void native_world_stereo(Matrix &out,const Matrix &flat,float slider,bool right){
 // Distant scenery sits behind the LCD; nearby geometry crosses its plane.
 // Leave depth and W untouched, including CPU-clipped and GPU palette draws.
 native_depth_stereo(out,flat,slider,right,-200.f*native_stereo_profile::separation);
}

/* Infinity backdrops use the world's far disparity with no eye translation.
 * Cover only the backdrop to prevent a strip at either shifted edge. */
template<typename Matrix>
inline void native_sky_stereo(Matrix &out,const Matrix &flat,float slider,bool right){
 native_world_stereo(out,flat,slider,right);
 if(!(slider>0.f))slider=0.f;
 if(slider>2.f)slider=2.f;
 out.r[1].x*=1.f+native_stereo_profile::separation*slider;
 out.r[1].y*=1.f+native_stereo_profile::separation*slider;
 out.r[1].z*=1.f+native_stereo_profile::separation*slider;
}

/* Shader applies this bound after the perspective transform. Six LCD pixels
 * per eye at full slider avoids excessive near-plane separation. */
inline float native_stereo_bounded_shift(float shift,float w,float slider){
 float limit=.03f*slider*w;
 return shift>limit?limit:shift< -limit?-limit:shift;
}

/* World convergence defaults to 3000 camera-W units and adapts to scene
 * depth. First-person weapons retain their separate near-camera profile. */
inline float native_stereo_eye_translation(float slider,bool right,bool weapon,float convergence=native_stereo_profile::manual_convergence){
 return (right?1.f:-1.f)*slider*(weapon?.8f:native_stereo_profile::separation*convergence);
}
