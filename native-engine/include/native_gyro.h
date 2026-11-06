#pragma once
#include <stdint.h>
/* Integrate angular velocity, never absolute orientation or stale frame time. */
struct NativeGyro {uint64_t time;float yaw,pitch,bias_yaw,bias_pitch,filtered_yaw,filtered_pitch;};
static inline float native_gyro_abs(float v){return v<0?-v:v;}
static inline float native_gyro_bound(float v,float limit){return v>limit?limit:v< -limit?-limit:v;}
static inline void native_gyro_sample(struct NativeGyro *g,uint64_t now,float yaw,float pitch,int active){
 uint64_t previous=g->time;g->time=now;
 if(!previous || now<=previous || now-previous>100000 || !__builtin_isfinite(yaw) || !__builtin_isfinite(pitch)){
  g->yaw=g->pitch=g->filtered_yaw=g->filtered_pitch=0;return;
 }
 float dt=(now-previous)*.000001f;
 if(!active){
  /* Calibrate only while motion control is disengaged. Learning during aim
   * would incorrectly absorb deliberate slow corrections into the bias. */
  if(native_gyro_abs(yaw)<1.f && native_gyro_abs(pitch)<1.f){
   float a=dt/(1.f+dt);g->bias_yaw+=a*(yaw-g->bias_yaw);g->bias_pitch+=a*(pitch-g->bias_pitch);
  }
  g->yaw=g->pitch=g->filtered_yaw=g->filtered_pitch=0;return;
 }
 yaw-=g->bias_yaw;pitch-=g->bias_pitch;
 if(native_gyro_abs(yaw)<.12f)yaw=0;
 if(native_gyro_abs(pitch)<.12f)pitch=0;
 float a=dt/(.012f+dt);
 g->filtered_yaw+=a*(yaw-g->filtered_yaw);g->filtered_pitch+=a*(pitch-g->filtered_pitch);
 g->yaw=native_gyro_bound(g->yaw+native_gyro_bound(g->filtered_yaw,600)*dt,15);
 g->pitch=native_gyro_bound(g->pitch+native_gyro_bound(g->filtered_pitch,600)*dt,15);
}
