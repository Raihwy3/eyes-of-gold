#ifndef EOG_NATIVE_WATCH_ENVIRONMENT_H
#define EOG_NATIVE_WATCH_ENVIRONMENT_H
/* Retain the level's tile shade throughout opening, open and closing states. */
static inline unsigned native_watch_environment_rgba(unsigned r,unsigned g,unsigned b,unsigned a) {
    return (r<<24)|(g<<16)|(b<<8)|a;
}
#endif
