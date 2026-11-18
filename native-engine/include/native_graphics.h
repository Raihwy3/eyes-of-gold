#ifndef NATIVE_GRAPHICS_H
#define NATIVE_GRAPHICS_H
#ifdef __cplusplus
extern "C" {
#endif
int native_graphics_init(void);
/* Startup-only diagnostic: background CPU reference, other GPU paths kept. */
void native_graphics_reference_background(int enabled);
void native_graphics_set_native_resolution(int width,int height);
void native_graphics_render(void *commands);
void native_graphics_shutdown(void);
void native_graphics_stop_presenting(void);
unsigned native_graphics_frames(void);
unsigned native_graphics_triangles(void);
unsigned native_graphics_uploads(void);
int native_scene_load(const char *path);
void native_scene_frame(unsigned frame, unsigned mode);
void native_scene_destroy(void);
#ifdef __cplusplus
}
#endif
#endif
