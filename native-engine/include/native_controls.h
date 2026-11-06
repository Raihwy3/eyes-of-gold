#ifndef EOG_NATIVE_CONTROLS_H
#define EOG_NATIVE_CONTROLS_H
struct MoveData;
float native_controls_look_rate(float rate);
void native_controls_adjust_look(float *yaw,float *pitch);
void native_controls_apply_gyro(void);
void native_controls_scope_head(void);
int native_controls_aim_turn(float *x,float *y);
void native_controls_apply(struct MoveData *movement);
int native_controls_restore(int original);
int native_game_input_gameplay(void);
#endif
