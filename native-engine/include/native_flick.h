#ifndef EOG_NATIVE_FLICK_H
#define EOG_NATIVE_FLICK_H
#include <stdint.h>
#include <math.h>
/* Capture a short outward gesture before choosing its direction. Only strong
 * deflections rotate a held gesture: returning to centre must not steer it. */
struct NativeFlick {
 uint64_t last,pressed;float angle,candidate,remaining,pending;
 unsigned remaining_us;int active,armed,tracking,peak,choosing;
};
static inline float native_flick_wrap(float a){if(a>180)a-=360;else if(a< -180)a+=360;return a;}
static inline void native_flick_commit(struct NativeFlick *f){
 float a=f->candidate,cardinal=roundf(a/90.f)*90.f;
 if(fabsf(a-cardinal)<12.f)a=cardinal;
 f->remaining+=a;f->remaining_us=100000;f->choosing=0;
}
static inline void native_flick_sample(struct NativeFlick *f,uint64_t now,int x,int y,int enabled){
 uint64_t dt=f->last && now>=f->last?now-f->last:0;
 if(!enabled || dt>250000){*f=(struct NativeFlick){0};f->last=now;return;}
 f->last=now;
 if(x>156)x=156;else if(x< -156)x=-156;
 if(y>156)y=156;else if(y< -156)y=-156;
 int radius=x*x+y*y;float angle=radius>=48*48?atan2f((float)x,(float)y)*57.2957795131f:f->angle;
 if(radius<20*20){
   if(f->choosing)native_flick_commit(f);
   f->active=f->tracking=0;f->armed=1;
 }else if(!f->active && f->armed && radius>=48*48){
   f->active=f->choosing=1;f->armed=0;f->pressed=now;f->candidate=f->angle=angle;f->peak=radius;f->tracking=0;
 }else if(f->active){
   if(f->choosing){
     if(radius>f->peak){f->candidate=angle;f->peak=radius;}
     if(now-f->pressed>=25000){native_flick_commit(f);f->angle=angle;f->tracking=radius>=64*64;}
   }else if(radius>=64*64){
     float delta=native_flick_wrap(angle-f->angle);
     if(f->tracking && fabsf(delta)>=3.f){f->pending+=delta;f->angle=angle;}
     else if(!f->tracking)f->angle=angle;
     f->tracking=1;
   }else f->tracking=0;
 }
 if(f->remaining_us){unsigned used=dt<f->remaining_us?(unsigned)dt:f->remaining_us;
   float step=f->remaining*(float)used/f->remaining_us;
   f->remaining_us-=used;f->remaining-=step;f->pending+=step;
 }
 if(f->pending>360)f->pending=360;else if(f->pending< -360)f->pending=-360;
}
#endif
