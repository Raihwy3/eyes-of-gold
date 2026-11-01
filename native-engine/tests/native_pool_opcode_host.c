#include <ultra64.h>
#include <bondtypes.h>
#include "game/bg.h"
#include <stdio.h>
#include <stdlib.h>
extern char g_ModelHitEntries[];
ModelHitEntry *g_ModelHitFreeList;
extern void initModelHitEntryFreeList(void);
extern void bgBuildRoomVtxBounds(s32);
static u8 bounds_storage[256] __attribute__((aligned(16)));
void *memaAlloc(u32 bytes) {
    if (bytes>sizeof bounds_storage) abort();
    return bounds_storage;
}
void osSyncPrintf(const char *fmt, ...) { (void)fmt; abort(); }
int main(void) {
    Gfx command = {0};
    for (int op=0;op<256;op++) {
        command.words.w0=(u32)op<<24;
        if (command.dma.cmd != (s8)op) abort();
    }
    command.words.w0=(u32)(u8)G_ENDDL<<24;
    if (command.dma.cmd != G_ENDDL) abort();
    command.words.w0=(u32)(u8)G_VTX<<24;
    if (command.dma.cmd != G_VTX) abort();
    ModelHitEntry *entries=(ModelHitEntry*)g_ModelHitEntries;
    for (unsigned pass=0;pass<2;pass++) {
        initModelHitEntryFreeList();
        if (g_ModelHitFreeList != entries) abort();
        for (unsigned i=0;i<600;i++) {
            if (i<599 && entries[i].next != &entries[i+1]) abort();
            if (i>0 && entries[i].prev != &entries[i-1]) abort();
        }
    }
    Gfx gdl[2] = {0};
    Vtx vertices[3] = {0};
    bg_room_data rooms[2] = {0};
    ptr_bgdata_room_fileposition_list=rooms;
    vertices[0].v.ob[0]=-10; vertices[1].v.ob[0]=20;
    vertices[0].v.ob[1]=-30; vertices[2].v.ob[1]=40;
    vertices[1].v.ob[2]=-50; vertices[2].v.ob[2]=60;
    gSPVertex(&gdl[0],0,3,0);
    gSPEndDisplayList(&gdl[1]);
    g_BgRoomInfo[1].ptr_expanded_mapping_info=gdl;
    g_BgRoomInfo[1].vertices=vertices;
    g_BgRoomInfo[1].usize_point_index_binary=sizeof vertices;
    bgBuildRoomVtxBounds(1);
    RoomVtxBatchBounds *bounds=g_BgRoomInfo[1].vtx_batch_bounds;
    if (!bounds || g_BgRoomInfo[1].num_vtx_batch_bounds!=1 || bounds[0].xmin!=-10 || bounds[0].xmax!=20 || bounds[0].ymin!=-30 || bounds[0].ymax!=40 || bounds[0].zmin!=-50 || bounds[0].zmax!=60) abort();
    puts("PASS original room vertex-bounds walker terminates at G_ENDDL and computes batch bounds");
    puts("PASS signed GBI opcode decoding for all 256 bytes and original 600-entry model-hit pool links");
    return 0;
}
