#ifndef EOG_NATIVE_FLICK_H
#define EOG_NATIVE_FLICK_H
#include <stdint.h>
#include <math.h>
/* Flick timing and rotation smoothing adapted from JoyShockMapper:
 * Copyright 2018-2021 Julian "Jibb" Smart and contributors (MIT).
 * Revision 980dc52c09fbdfb3378b04a8a1895ea3157c29e1, main.cpp:
 * handleFlickStick and GetSmoothedStickRotation. See licenses/JoyShockMapper.txt.
 * 3DS adaptation: camera degrees instead of mouse units, pressure thresholds,
 * neutral debounce and draining the smoothing tail even after release. */
/* A flick turns over 100ms; subsequent angular motion is independent of it.
 * The pressure nub needs release hysteresis rather than a second, stronger
 * rotation threshold. A brief neutral dropout must not start another flick. */
struct NativeFlick {
 uint64_t last,neutral_since,started_flick;float angle,remaining,pending;
 float delta_flick,flick_percent_done,samples[64];int front_sample,sample_count;
 unsigned remaining_us;int active,armed,tracking,neutral_samples;
};
static inline float native_flick_wrap(float a){if(a>180)a-=360;else if(a< -180)a+=360;return a;}
/* JSM blends small angular changes into a circular moving average while
 * large changes pass through immediately. Thresholds are 0.08/0.16 radians. */
static inline float native_flick_smooth(struct NativeFlick *f,float value){
 if(--f->front_sample<0)f->front_sample=63;
 float immediate=(fabsf(value)-0.08f*57.2957795131f)/(0.08f*57.2957795131f);
 if(immediate<0)immediate=0;else if(immediate>1)immediate=1;
 f->samples[f->front_sample]=value*(1-immediate);
 float result=0;
 for(int i=0;i<f->sample_count;i++)result+=f->samples[(f->front_sample+i)%64]/f->sample_count;
 return result+value*immediate;
}
static inline void native_flick_sample(struct NativeFlick *f,uint64_t now,int x,int y,int enabled){
 uint64_t dt=f->last && now>=f->last?now-f->last:0;
 if(!enabled || (f->last && now<f->last) || dt>250000){*f=(struct NativeFlick){0};f->last=now;return;}
 f->last=now;
 /* JSM uses absolute elapsed time and the difference between successive
  * quadratic ease-out values, independent of game frame rate. */
 if(f->remaining_us){
   float progress=(float)(now-f->started_flick)/100000.f;
   if(progress>1)progress=1;
   float old=1-f->flick_percent_done,new_value=1-progress;
   f->pending+=(old*old-new_value*new_value)*f->delta_flick;
   f->flick_percent_done=progress;
   f->remaining=f->delta_flick*new_value*new_value;
   f->remaining_us=progress>=1?0:(unsigned)(100000-(now-f->started_flick));
 }
 float rotation=0;
 if(x>156)x=156;else if(x< -156)x=-156;
 if(y>156)y=156;else if(y< -156)y=-156;
 int radius=x*x+y*y;
 if(radius<20*20){
   f->tracking=0;
   if(!f->active)f->armed=1;
   else {
     if(!f->neutral_samples)f->neutral_since=now;
     f->neutral_samples++;
     if(f->neutral_samples>=2 && now-f->neutral_since>=20000){f->active=0;f->armed=1;}
   }
 }else {
   f->neutral_samples=0;
   if(!f->active && f->armed && radius>=48*48){
     float a=atan2f((float)x,(float)y)*57.2957795131f;
     f->angle=a;
     /* Finish any outstanding turn rather than delaying it a second time. */
     f->pending+=f->remaining;
     /* Preserve queued smoothing when a fast second flick begins. */
     for(int i=0;i<f->sample_count;i++)f->pending+=native_flick_smooth(f,0);
     for(int i=0;i<64;i++)f->samples[i]=0;
     f->front_sample=0;
     f->sample_count=dt?(int)(64000/dt):4;
     if(f->sample_count<1)f->sample_count=1;else if(f->sample_count>64)f->sample_count=64;
     f->started_flick=now;f->delta_flick=f->remaining=a;
     f->flick_percent_done=0;f->remaining_us=100000;
     f->active=f->tracking=1;f->armed=0;
   }else if(f->active && radius>=48*48){
     float a=atan2f((float)x,(float)y)*57.2957795131f;
     if(f->tracking)rotation=native_flick_wrap(a-f->angle);
     f->angle=a;f->tracking=1;
   }else f->tracking=0;
 }
 if(f->sample_count)f->pending+=native_flick_smooth(f,rotation);
 if(f->pending>360)f->pending=360;else if(f->pending< -360)f->pending=-360;
}
#endif
