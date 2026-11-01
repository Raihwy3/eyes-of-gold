#ifndef EOG_NATIVE_SCENERY_BATCHES_H
#define EOG_NATIVE_SCENERY_BATCHES_H
#include <cstdint>
#include <cstring>
/* Compile bounded consecutive TRI1/TRI4 runs. Copy command content: no pointers
 * into unloadable room data or assumptions that an address stays unchanged. */
struct NativeSceneryBatches {
    static constexpr unsigned max_commands=32,max_triangles=128;
    struct Word {uint32_t w0,w1;};
    struct Triangle {uint8_t a,b,c;};
    struct Entry {unsigned hash,words,count;Word commands[max_commands];Triangle triangles[max_triangles];};
    Entry entries[64]{};
    unsigned hits=0,builds=0;
    void begin_frame(){hits=builds=0;}
    const Entry &get(const Word *words,unsigned count,uint8_t tri1,uint8_t tri4) {
        uint32_t hash=count;
        for(unsigned i=0;i<count;i++) {
            hash=(hash^words[i].w0)*16777619u;
            hash=(hash^words[i].w1)*16777619u;
        }
        hash^=hash>>16;auto &entry=entries[hash%64];
        if(entry.words==count && entry.hash==hash && !memcmp(entry.commands,words,count*sizeof(Word))) {hits++;return entry;}
        entry.hash=hash;entry.words=count;entry.count=0;memcpy(entry.commands,words,count*sizeof(Word));
        for(unsigned i=0;i<count;i++) {
            const auto &w=words[i];unsigned op=w.w0>>24;
            if(op==tri1)entry.triangles[entry.count++]={uint8_t(((w.w1>>16)&255)/10),uint8_t(((w.w1>>8)&255)/10),uint8_t((w.w1&255)/10)};
            else if(op==tri4)for(unsigned t=0;t<4;t++) {
                uint8_t a=(w.w1>>(8*t))&15,b=(w.w1>>(8*t+4))&15,c=(w.w0>>(4*t))&15;
                if(a || b || c)entry.triangles[entry.count++]={a,b,c};
            }
        }
        builds++;return entry;
    }
};
static_assert(sizeof(NativeSceneryBatches)<=64*1024,"Compiled scenery batch budget");
#endif
