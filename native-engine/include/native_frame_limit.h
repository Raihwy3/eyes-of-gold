#ifndef EOG_FRAME_LIMIT_H
#define EOG_FRAME_LIMIT_H
#include <stdint.h>
struct NativeFrameLimit {uint64_t deadline;int previous;};
static inline uint64_t native_frame_wait(struct NativeFrameLimit *s,uint64_t now,int fps) {
    if(fps!=s->previous || now>s->deadline+100000)s->deadline=now;
    return now<s->deadline?s->deadline-now:0;
}
static inline void native_frame_started(struct NativeFrameLimit *s,uint64_t now,int fps) {
    s->deadline=now+1000000/fps;s->previous=fps;
}
#endif
