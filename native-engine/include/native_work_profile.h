#ifndef EOG_NATIVE_WORK_PROFILE_H
#define EOG_NATIVE_WORK_PROFILE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum NativeWorkStage {
    NATIVE_WORK_MAIN, NATIVE_WORK_WAIT_GFX, NATIVE_WORK_WAIT_VI,
    NATIVE_WORK_SCENE, NATIVE_WORK_RENDER, NATIVE_WORK_RENDER_IDLE,
    NATIVE_WORK_FPS_WAIT, NATIVE_WORK_GPU_WAIT, NATIVE_WORK_SUBMIT,
    NATIVE_WORK_STAGE_LOAD, NATIVE_WORK_STAGE_COUNT
};
/* Optional platform hooks. Host checks and logging-disabled builds do no work. */
uint64_t native_work_profile_clock(void) __attribute__((weak));
void native_work_profile_record(unsigned stage,uint64_t begin,uint64_t end) __attribute__((weak));
static inline uint64_t native_work_profile_begin(void) {
    return native_work_profile_clock?native_work_profile_clock():0;
}
static inline void native_work_profile_end(unsigned stage,uint64_t begin) {
    if(begin && native_work_profile_record)
        native_work_profile_record(stage,begin,native_work_profile_clock());
}
/* One accumulator per stage per worker. Never shared across threads.
 * Durations are wall time, including preemption; nested rows must not be added. */
struct NativeWorkWindow {
    uint64_t begin,total,peak;unsigned count;
};
static inline int native_work_window_add(struct NativeWorkWindow *w,uint64_t begin,uint64_t end) {
    if(!begin || end<begin)return 0;
    if(!w->count)w->begin=begin;
    uint64_t duration=end-begin;
    w->total+=duration;if(duration>w->peak)w->peak=duration;w->count++;
    return end-w->begin>=5000000;
}
#ifdef __cplusplus
}
#endif
#endif
