#pragma once
/* SPDX-License-Identifier: MIT
 * Smooth shapes and coverage adapted from Conker: 3DS Recomp's rt64-3ds
 * renderer (RT64 Contributors, Raihwy3 and contributors). See
 * vendor/conker-reticle/source.json and distribution/licenses/conker-reticle.txt.
 * Generate immutable white/black RGBA masks once; the game supplies tint/alpha.
 */
#include <math.h>
#include <stdint.h>
#ifndef __cplusplus
/* The engine math wrapper omits these standard C99 declarations. */
float hypotf(float,float);
float fmaxf(float,float);
float fminf(float,float);
long lroundf(float);
#endif
#define NATIVE_CROSSHAIR_TEX_SIZE 64
static inline float native_reticle_box(float px,float py,float bx,float by){
 float dx=fabsf(px)-bx,dy=fabsf(py)-by;
 return hypotf(fmaxf(dx,0.f),fmaxf(dy,0.f))+fminf(fmaxf(dx,dy),0.f);
}
static inline float native_reticle_smooth(float edge0,float edge1,float x){
 float t=fminf(fmaxf((x-edge0)/(edge1-edge0),0.f),1.f);
 return t*t*(3.f-2.f*t);
}
static inline uint32_t native_crosshair_sdf_pixel(int style,int x,int y){
 float px=(float)x-31.5f,py=(float)y-31.5f,r=hypotf(px,py),d=100.f;
 if(style==1)d=r-5.2f;
 else if(style==2 || style==3 || style==6){
  float dl=native_reticle_box(px+15.5f,py,6.8f,1.9f);
  float dr=native_reticle_box(px-15.5f,py,6.8f,1.9f);
  float db=native_reticle_box(px,py-15.5f,1.9f,6.8f);
  d=fminf(fminf(dl,dr),db);
  if(style!=6)d=fminf(d,native_reticle_box(px,py+15.5f,1.9f,6.8f));
  if(style==3)d=fminf(d,r-3.3f);
 }else if(style==4 || style==5){
  d=fabsf(r-17.5f)-2.1f;
  if(style==5)d=fminf(d,r-3.3f);
 }else if(style==7){
  float man=(fabsf(px)+fabsf(py))*.70710678f;
  d=fabsf(man-13.5f)-2.1f;
 }
 float fill=1.f-native_reticle_smooth(-1.2f,1.2f,d);
 float outline=(1.f-native_reticle_smooth(-1.2f,1.2f,d-2.6f))*.78f;
 float alpha=fill+outline*(1.f-fill);
 if(alpha<=.002f)return 0;
 unsigned gray=(unsigned)lroundf(255.f*fill/alpha),a=(unsigned)lroundf(alpha*255.f);
 return (gray<<24)|(gray<<16)|(gray<<8)|a;
}
static inline void native_crosshair_sdf_generate(int style,uint32_t *pixels){
 for(int y=0;y<NATIVE_CROSSHAIR_TEX_SIZE;y++)for(int x=0;x<NATIVE_CROSSHAIR_TEX_SIZE;x++)
  pixels[y*NATIVE_CROSSHAIR_TEX_SIZE+x]=native_crosshair_sdf_pixel(style,x,y);
}
