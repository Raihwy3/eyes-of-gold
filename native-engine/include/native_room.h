#ifndef NATIVE_ROOM_H
#define NATIVE_ROOM_H
#ifdef __cplusplus
extern "C" {
#endif
int native_room_load(const char *path);
void native_room_render(float yaw, float pitch, float distance);
int native_world_load(const char *path);
void native_world_reset(void);
void native_world_move(float sideways, float forward, float yaw);
void native_world_update(float sideways, float forward, float yaw,
                         float seconds);
void native_world_render(float yaw, float pitch);
unsigned native_world_room(void);
void native_world_position(float *xyz);
unsigned native_room_triangles(void);
unsigned native_room_textures(void);
#ifdef __cplusplus
}
#endif
#endif
