#ifndef EOG_NATIVE_DYNAMIC_LOD_H
#define EOG_NATIVE_DYNAMIC_LOD_H
#include <stdint.h>
/* Half-second windows, one-second degradation and three-second recovery.
 * Recovery requires unpaced work headroom as well as reaching the FPS cap. */
typedef struct NativeDynamicLod {
    uint64_t last,elapsed,work,poor,good;
    unsigned frames;int strength,enabled,target;
} NativeDynamicLod;
static inline int native_dynamic_lod_step(NativeDynamicLod *s,uint64_t now,unsigned work_us,int fps,int enabled,int gameplay) {
    if(!s->strength)s->strength=1;
    if(!enabled){s->enabled=0;s->last=s->elapsed=s->work=s->poor=s->good=0;s->frames=0;return s->strength;}
    if(!s->enabled){s->strength=1;s->enabled=1;s->last=0;}
    if(!gameplay || s->target!=fps){s->last=s->elapsed=s->work=s->poor=s->good=0;s->frames=0;s->target=fps;}
    if(!gameplay)return s->strength;
    if(!s->last){s->last=now;return s->strength;}
    uint64_t dt=now>s->last?now-s->last:0;s->last=now;
    if(dt<1000 || dt>500000){s->elapsed=s->work=s->poor=s->good=0;s->frames=0;return s->strength;}
    s->elapsed+=dt;s->work+=work_us;s->frames++;
    if(s->elapsed<500000)return s->strength;
    uint64_t interval=s->elapsed/s->frames,work=s->work/s->frames,budget=1000000u/(fps==30?30:60);
    if(interval>budget*112/100) {
        s->poor+=s->elapsed;s->good=0;
        if(s->poor>=1000000){if(s->strength<3)s->strength++;s->poor=0;}
    }else if(interval<=budget*106/100 && work<budget*75/100) {
        s->good+=s->elapsed;s->poor=0;
        if(s->good>=3000000){if(s->strength>1)s->strength--;s->good=0;}
    }else s->poor=s->good=0;
    s->elapsed=s->work=0;s->frames=0;return s->strength;
}
#endif
