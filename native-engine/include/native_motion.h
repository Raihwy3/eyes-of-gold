#ifndef EOG_NATIVE_MOTION_H
#define EOG_NATIVE_MOTION_H
#ifdef __cplusplus
extern "C" {
#endif
int native_motion_load(const char *path);
void native_motion_reset(void);
void native_motion_tick(float seconds, float forward, float sideways);
void native_motion_travel(float *xyz);
void native_motion_camera(float *position, float *look, float *up);
#ifdef __cplusplus
}
#endif
#endif
