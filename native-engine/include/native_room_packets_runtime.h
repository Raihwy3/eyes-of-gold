#ifndef EOG_NATIVE_ROOM_PACKETS_RUNTIME_H
#define EOG_NATIVE_ROOM_PACKETS_RUNTIME_H
#include <algorithm>
#include "native_scenery_batches.h"
#include "native_room_packet_scope.h"
// Generated data deliberately has a different include path from the scope ABI.
#ifdef __3DS__
#include "native_derived.h"
#else
#include <native_room_packets_data.h>
#endif
struct NativeRoomPackets {
    struct Binding {uintptr_t address=0,base=0;unsigned room=0,generation=0,bytes=0;const NativeSceneryBatches::Entry *entry=nullptr;};
    Binding bindings[2048]{};
    NativeRoomPacketScope current{};
    unsigned hits=0,binds=0,misses=0;
    void scope(const NativeRoomPacketScope *p){current=p?*p:NativeRoomPacketScope{};}
    void begin_frame(){scope(nullptr);hits=binds=misses=0;}
    const NativeSceneryBatches::Entry *get(const void *command,unsigned &count) {
        const uintptr_t at=(uintptr_t)command,base=(uintptr_t)current.base;
        if(!base || !current.generation || at<base || at-base>=current.bytes || (at-base)%sizeof(Gfx)){misses++;return nullptr;}
        const unsigned remaining=(current.bytes-(at-base))/sizeof(Gfx);
        auto &binding=bindings[(at>>3)%2048];
        if(binding.address==at && binding.base==base && binding.room==current.room && binding.generation==current.generation && binding.bytes==current.bytes) {
            if(binding.entry){count=binding.entry->words;hits++;return binding.entry;}
            misses++;return nullptr;
        }
        NativeSceneryBatches::Word words[32];unsigned n=0;
        const auto *g=(const Gfx *)command;
        while(n<32 && n<remaining) {
            unsigned op=g[n].words.w0>>24;if(op!=0xbf && op!=0xb1)break;
            words[n]={(uint32_t)g[n].words.w0,(uint32_t)g[n].words.w1};n++;
        }
        // Sorted by the entire original command sequence. Compare every word
        // on binding; subsequent frames use the load generation and pointer.
        auto compare=[&](const NativeSceneryBatches::Entry &entry) {
            unsigned common=std::min(n,entry.words);
            for(unsigned i=0;i<common;i++) {
                if(words[i].w0!=entry.commands[i].w0)return words[i].w0<entry.commands[i].w0?-1:1;
                if(words[i].w1!=entry.commands[i].w1)return words[i].w1<entry.commands[i].w1?-1:1;
            }
            return n<entry.words?-1:n>entry.words?1:0;
        };
        unsigned lo=0,hi=native_room_packet_count;
        while(lo<hi){unsigned mid=(lo+hi)/2;if(compare(native_room_packets[mid])>0)lo=mid+1;else hi=mid;}
        const auto *entry=lo<native_room_packet_count && compare(native_room_packets[lo])==0?&native_room_packets[lo]:nullptr;
        binding={at,base,current.room,current.generation,current.bytes,entry};
        if(entry){count=entry->words;binds++;return entry;}
        misses++;return nullptr;
    }
};
static_assert(sizeof(NativeRoomPackets)<100*1024,"Room packet binding memory budget");
#endif
