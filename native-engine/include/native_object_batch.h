#ifndef EOG_NATIVE_OBJECT_BATCH_H
#define EOG_NATIVE_OBJECT_BATCH_H
/* Included after the original PROPDEF constants. Render-only eligibility. */
static inline int native_object_batch_eligible(unsigned type,int secondary,int destroyed) {
    return !secondary && !destroyed && (type==PROPDEF_PROP || type==PROPDEF_VEHICHLE || type==PROPDEF_DOOR);
}
#endif
