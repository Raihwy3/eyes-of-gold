#ifndef EOG_NATIVE_RENDER_REFERENCE_H
#define EOG_NATIVE_RENDER_REFERENCE_H
#include <stdio.h>
/* Optional diagnostic package marker. Never changes saved preferences. */
static inline int native_render_marker_requested(const char *directory,const char *name) {
    char path[512];if(!directory)return 0;
    int n=snprintf(path,sizeof path,"%s/%s",directory,name);
    if(n<0 || n>=(int)sizeof path)return 0;
    FILE *file=fopen(path,"rb");if(!file)return 0;
    fclose(file);return 1;
}
static inline int native_reference_background_requested(const char *directory) {
    return native_render_marker_requested(directory,"reference-background.flag");
}
static inline int native_reference_background_enabled(const char *directory) {
    /* The CPU background path is the verified hardware baseline. */
    return !native_render_marker_requested(directory,"gpu-background.flag") ||
        native_reference_background_requested(directory);
}
#endif
