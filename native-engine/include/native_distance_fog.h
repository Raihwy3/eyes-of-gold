#ifndef EOG_NATIVE_DISTANCE_FOG_H
#define EOG_NATIVE_DISTANCE_FOG_H
#include <math.h>
#define NATIVE_DISTANCE_FOG_COMMAND 0x48
struct NativeDistanceFog {float mul,offset,end_ndc,end_world;unsigned colour;};
/* The original camera projection is retained. Only its render-time fog ramp
 * and clip plane change; game visibility/AI continue to use authored ranges. */
static inline int native_distance_fog_plan(float near_world,float far_world,float end_world,unsigned colour,struct NativeDistanceFog *out) {
    if(!(near_world>0 && far_world>near_world && end_world>near_world*2 && end_world<far_world))return 0;
    double n=near_world,f=far_world,e=end_world,s=fmax(n*1.01,e*.65);
    double a=(f+n)/(f-n),b=2*f*n/(f-n);
    double start=a-b/s,end=a-b/e;
    if(!(end>start))return 0;
    out->mul=(float)(255/(end-start));out->offset=(float)(-start*255/(end-start));
    out->end_ndc=(float)end;out->end_world=end_world;out->colour=colour;
    return 1;
}
static inline float native_distance_fog_factor(const struct NativeDistanceFog *p,float z,float w) {
    if(w<=0)return 0;
    float f=(z/w)*p->mul+p->offset;
    return f<0?0:f>255?255:f;
}
#define native_distance_fog_command(pkt,params) gDma0p((pkt),NATIVE_DISTANCE_FOG_COMMAND,(params),(params)?1:0)
#endif
