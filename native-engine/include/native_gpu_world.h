#ifndef NATIVE_GPU_WORLD_H
#define NATIVE_GPU_WORLD_H
/* Rows map model-space position to the same homogeneous clip coordinates
 * used by the reference renderer. Backend consumes/copies before return. */
struct NativeGpuWorld { float rows[4][4]; unsigned cull; };
#endif
