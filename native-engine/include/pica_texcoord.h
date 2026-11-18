#ifndef NATIVE_PICA_TEXCOORD_H
#define NATIVE_PICA_TEXCOORD_H
/* Fast3D supplies top-origin image coordinates; PICA samples T bottom-up.
 * Invert the whole padded GPU texture after scaling the image extent. */
inline float pica_texture_t(float t, unsigned image_height, unsigned gpu_height) {
  return 1.f - t * image_height / gpu_height;
}
/* Padded GPU extents are powers of two; their reciprocal is exact. Keep
 * multiply order unchanged so this matches (coord * image) / gpu bitwise. */
inline float pica_texture_scaled(float coordinate,unsigned image,float inverse_gpu) {
  return (coordinate*image)*inverse_gpu;
}
inline float pica_lookup_t(float t) { return 1.f - t; }
#endif
