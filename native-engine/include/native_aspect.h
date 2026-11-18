#ifndef EOG_NATIVE_ASPECT_H
#define EOG_NATIVE_ASPECT_H
/* Physical top LCD: 400x240 = 5:3. 4:3 occupies 320x240, centred. */
static inline float native_aspect_ratio(int full) { return full==2?5.f/4.f:full==1?5.f/3.f:4.f/3.f; }
static inline unsigned native_aspect_width(int full) { return full==2?300u:full==1?400u:320u; }
static inline const char *native_aspect_label(int full) { return full==2?"5:4\n":full==1?"5:3\n":"4:3\n"; }
#endif
