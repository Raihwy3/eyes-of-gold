#pragma once
#include <opendsp.h>
typedef struct {uint32_t struct_size;float room_size,decay_time,damping,diffusion,width,level;} opendspReverbConfig;
Result opendspReverbSet(unsigned,const opendspReverbConfig *);
