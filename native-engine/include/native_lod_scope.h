#ifndef EOG_NATIVE_LOD_SCOPE_H
#define EOG_NATIVE_LOD_SCOPE_H
/* Frame-owned world-to-view scale. It removes camera scaling from clip W. */
#define NATIVE_LOD_SCOPE_COMMAND 0x4a
struct NativeLodScope {float world_to_view;};
#define native_lod_scope(pkt,params) gDma0p((pkt),NATIVE_LOD_SCOPE_COMMAND,(params),(params)?1:0)
#endif
