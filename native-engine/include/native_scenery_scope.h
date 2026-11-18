#ifndef EOG_NATIVE_SCENERY_SCOPE_H
#define EOG_NATIVE_SCENERY_SCOPE_H
/* Opaque BG=1, props=2, actors=3, vehicles=4. No allocation ownership. */
#define NATIVE_SCENERY_COMMAND 0x49
#define native_scenery_scope(pkt,on) gDma0p((pkt),NATIVE_SCENERY_COMMAND,0,(on)?1:0)
#define native_object_scope(pkt,on) gDma0p((pkt),NATIVE_SCENERY_COMMAND,0,(on)?2:0)
#define native_character_scope(pkt,on) gDma0p((pkt),NATIVE_SCENERY_COMMAND,0,(on)?3:0)
#define native_faded_character_scope(pkt,on) gDma0p((pkt),NATIVE_SCENERY_COMMAND,0,(on)?6:0)
#define native_vehicle_scope(pkt,on) gDma0p((pkt),NATIVE_SCENERY_COMMAND,0,(on)?4:0)
#define native_simple_glass_scope(pkt,on) gDma0p((pkt),NATIVE_SCENERY_COMMAND,0,(on)?5:0)
#endif
