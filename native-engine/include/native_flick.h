#ifndef EOG_NATIVE_FLICK_H
#define EOG_NATIVE_FLICK_H
#include <stdint.h>
#include <math.h>
/* Raw C-stick units. Low thresholds suit the pressure-sensitive 3DS nub;
 * releasing requires a real return to centre, rather than a pressure dip.
 * Initial turns finish over 100ms; edge rotation tracks one-to-one degrees. */
struct NativeFlick { uint64_t last;float angle,remaining,pending;unsigned remaining_us;int active,armed; };
static inline float native_flick_wrap(float a) {
    if(a>180.f)a-=360.f;else if(a< -180.f)a+=360.f;return a;
}
static inline void native_flick_sample(struct NativeFlick *f,uint64_t now,int x,int y,int enabled) {
    uint64_t dt=f->last && now>=f->last?now-f->last:0;
    if(!enabled || dt>250000){*f=(struct NativeFlick){0};f->last=now;return;}
    f->last=now;
    if(x>156)x=156;else if(x< -156)x=-156;
    if(y>156)y=156;else if(y< -156)y=-156;
    int radius=x*x+y*y;
    if(radius<20*20){f->active=0;f->armed=1;}
    else if(radius>=48*48 || f->active) {
        float angle=atan2f((float)x,(float)y)*57.2957795131f;
        if(f->active)f->pending+=native_flick_wrap(angle-f->angle);
        else if(f->armed){f->remaining+=angle;f->remaining_us=100000;f->active=1;f->armed=0;}
        f->angle=angle;
    }
    /* At most 100ms of bounded rotation can be queued at each HID sample.
     * Finish a quick flick even if the player immediately releases the stick. */
    if(f->remaining_us) {
        unsigned used=dt<f->remaining_us?(unsigned)dt:f->remaining_us;
        float step=f->remaining*(float)used/f->remaining_us;
        f->remaining_us-=used;f->remaining-=step;f->pending+=step;
    }
    if(f->pending>360)f->pending=360;else if(f->pending< -360)f->pending=-360;
}
#endif
