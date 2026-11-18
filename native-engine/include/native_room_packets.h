#ifndef EOG_NATIVE_ROOM_PACKET_SCOPE_H
#define EOG_NATIVE_ROOM_PACKET_SCOPE_H
#include <ultra64.h>
#include "native_room_packet_scope.h"
#ifdef __cplusplus
extern "C" {
#endif
extern void native_room_packets_loaded(unsigned) __attribute__((weak));
extern Gfx *native_room_packets_begin(Gfx *,unsigned,const void *,unsigned) __attribute__((weak));
#ifdef __cplusplus
}
#endif
#endif
