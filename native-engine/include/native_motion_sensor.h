#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void native_motion_reset(void);
void native_motion_set_filter(int enabled);
void native_motion_calibrate(void);
/* 1: waiting for stillness; 2: sampling; 0: idle; -1: completed; -2: timed out. */
int native_motion_calibration_status(void);
int native_motion_calibration_progress(void);
void native_motion_sample(uint64_t now,float gx,float gy,float gz,float ax,float ay,float az,int active,int space,int axis,float *yaw,float *pitch);
#ifdef __cplusplus
}
#endif
