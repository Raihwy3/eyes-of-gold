#pragma once
#include <3ds.h>
#define OPENDSP_CAP_REVERB 4
Result opendspInit(void);void opendspExit(void);Result opendspGetCapabilities(uint32_t *);
typedef struct {uint32_t struct_size,frames,late_frames,max_backlog,fifo_empty;float load,load_max;uint32_t frame_ticks,frame_ticks_min,frame_ticks_max;} opendspStats;
Result opendspGetStats(opendspStats *,bool);
