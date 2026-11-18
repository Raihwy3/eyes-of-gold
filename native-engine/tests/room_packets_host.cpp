#include <cstdint>
struct Gfx {struct {uintptr_t w0,w1;} words;};
#include "native_room_packets_runtime.h"
#include <cassert>
#include <cstdio>
static NativeRoomPackets packets;
int main() {
    NativeSceneryBatches original;
    Gfx commands[33]{};unsigned total=0;
    for(unsigned i=0;i<native_room_packet_count;i++) {
        const auto &e=native_room_packets[i];
        for(unsigned j=0;j<e.words;j++){commands[j].words.w0=e.commands[j].w0;commands[j].words.w1=e.commands[j].w1;}
        commands[e.words].words.w0=0xb8000000u;
        NativeRoomPacketScope scope{commands,unsigned((e.words+1)*sizeof(Gfx)),17,i+1};packets.scope(&scope);
        unsigned count=0;
        auto *found=packets.get(commands,count);
        assert(found==&e && count==e.words);
        auto &reference=original.get(e.commands,e.words,0xbf,0xb1);
        assert(reference.count==found->count && !memcmp(reference.triangles,found->triangles,e.count*sizeof(e.triangles[0])));
        unsigned binds=packets.binds;
        for(unsigned frame=0;frame<10;frame++){assert(packets.get(commands,count)==found);assert(packets.binds==binds);}
        total+=e.count;
        // Reused room memory is not an identity: only a load generation is.
        commands[0].words.w0=0;
        scope.generation+=10000;packets.scope(&scope);
        assert(!packets.get(commands,count));
        scope.bytes=sizeof(Gfx)-1;packets.scope(&scope);assert(!packets.get(commands,count));
        packets.scope(nullptr);assert(!packets.get(commands,count));
    }
    printf("PASS offline room packets: %u programs / %u triangles exactly match original decoder; direct replay, reload generations, truncated buffers and unscoped fallback\n",native_room_packet_count,total);
}
