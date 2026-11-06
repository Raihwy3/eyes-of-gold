#pragma once
/* Eye convergence shifts clip X by a W term. The shaders add the eye
 * translation separately, before perspective division, using camera-space W. */
template<typename Matrix>
inline void native_depth_stereo(Matrix &out,const Matrix &flat,float slider,bool right,float base_pixels){
 if(!(slider>0.f))slider=0.f;
 if(slider>1.f)slider=1.f;
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
 native_depth_stereo(out,flat,slider,right,-2.f);
}

/* Shader applies this bound after the perspective transform. Six LCD pixels
 * per eye at full slider avoids excessive near-plane separation. */
inline float native_stereo_bounded_shift(float shift,float w,float slider){
 float limit=.03f*slider*w;
 return shift>limit?limit:shift< -limit?-limit:shift;
}

/* Pixel-distance product: world convergence at 600 game units; first-person
 * weapons use the game's separate near-camera projection. No Z-buffer proxy. */
inline float native_stereo_eye_translation(float slider,bool right,bool weapon){
 return (right?1.f:-1.f)*slider*(weapon?160.f:1200.f)/200.f;
}
