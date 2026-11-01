/* Sparse game-thread wall-time probes: one of every 67 guard calls. */
#include <stdint.h>
#include "system.h"
#include "native_guard_activity.h"
static int native_guard_sample;
static unsigned native_guard_tick_calls,native_guard_render_calls;
static uint64_t native_guard_us[6];
static unsigned native_guard_samples[6];
static uint64_t native_guard_begin(void){return native_guard_sample?sysGetMicroseconds():0;}
static void native_guard_end(unsigned stage,uint64_t start){if(start){native_guard_us[stage]+=sysGetMicroseconds()-start;native_guard_samples[stage]++;}}
#define NATIVE_GUARD_TIME(stage,statement) do {uint64_t native_start=native_guard_begin();statement;native_guard_end(stage,native_start);} while(0)
void native_guard_profile_report(void) {
    native_guard_activity_report();
    sysLogPrintf(LOG_NOTE,"Guard work (sampled us/call): AI=%.1f/%u moveAnim=%.1f/%u skeleton=%.1f/%u drawLists=%.1f/%u tickTotal=%.1f/%u renderLists=%.1f/%u (nested, includes preemption)",
        native_guard_samples[0]?(double)native_guard_us[0]/native_guard_samples[0]:0.,native_guard_samples[0],
        native_guard_samples[1]?(double)native_guard_us[1]/native_guard_samples[1]:0.,native_guard_samples[1],
        native_guard_samples[2]?(double)native_guard_us[2]/native_guard_samples[2]:0.,native_guard_samples[2],
        native_guard_samples[3]?(double)native_guard_us[3]/native_guard_samples[3]:0.,native_guard_samples[3],
        native_guard_samples[4]?(double)native_guard_us[4]/native_guard_samples[4]:0.,native_guard_samples[4],
        native_guard_samples[5]?(double)native_guard_us[5]/native_guard_samples[5]:0.,native_guard_samples[5]);
    for(unsigned i=0;i<6;i++){native_guard_us[i]=0;native_guard_samples[i]=0;}
}
