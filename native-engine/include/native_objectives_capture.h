#ifndef EOG_OBJECTIVES_CAPTURE_H
#define EOG_OBJECTIVES_CAPTURE_H
#include <ultra64.h>
#include "native_bottom.h"
struct fontchar;struct font;
static int native_objective_collecting,native_objective_overlap_value=-1;
static struct EogObjectiveLayout *native_objective_layout;
static inline int native_objective_is_collecting(void) {
#ifdef EOG_OBJECTIVE_CAPTURE_TEST
    return 1;
#else
    return native_objective_collecting;
#endif
}
static Gfx *native_objective_text(Gfx *,s32 *,s32 *,char *,struct fontchar *,struct font *,u32,s32,s32,u32,s32);
static Gfx *native_objective_blackbox(Gfx *,s32 *,s32 *,s32 *,s32 *);
static void native_objective_overlap(s32);
static void native_capture_objectives(struct EogObjectiveLayout *);
#endif
