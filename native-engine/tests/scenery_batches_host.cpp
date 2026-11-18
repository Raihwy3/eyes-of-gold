#include "native_scenery_batches.h"
#include <cassert>
#include <cstdio>
int main() {
    NativeSceneryBatches cache;
    using Word=NativeSceneryBatches::Word;
    Word commands[32]={{0xbf000000,0x000a141e},{0xb1000520,0x00ba9831}};
    // TRI1 indices (1,2,3), then TRI4 (1,3,0),(8,9,2),(10,11,5).
    auto first=cache.get(commands,2,0xbf,0xb1);
    assert(first.count==4 && first.triangles[0].a==1 && first.triangles[0].b==2 && first.triangles[0].c==3);
    assert(first.triangles[1].a==1 && first.triangles[1].b==3 && first.triangles[1].c==0);
    assert(first.triangles[2].a==8 && first.triangles[2].b==9 && first.triangles[2].c==2);
    assert(first.triangles[3].a==10 && first.triangles[3].b==11 && first.triangles[3].c==5);
    cache.get(commands,2,0xbf,0xb1);assert(cache.hits==1 && cache.builds==1);
    commands[0].w1=0x001e140a;
    auto changed=cache.get(commands,2,0xbf,0xb1);
    assert(changed.triangles[0].a==3 && changed.triangles[0].c==1);
    // The same backing array is repeatedly replaced, as room data can be.
    for(unsigned i=0;i<1000;i++) {
        commands[0]={0xbf000000,((i%16)*10)<<16};
        const auto &entry=cache.get(commands,1,0xbf,0xb1);
        assert(entry.count==1 && entry.triangles[0].a==i%16);
    }
    for(auto &word:commands)word={0xb1001111,0x32323232};
    const auto &maximum=cache.get(commands,32,0xbf,0xb1);
    assert(maximum.words==32 && maximum.count==128);
    for(const auto &t:maximum.triangles)assert(t.a==2 && t.b==3 && t.c==1);
    auto before=cache.builds;cache.begin_frame();cache.get(commands,32,0xbf,0xb1);
    assert(before && cache.hits==1 && !cache.builds);
    puts("PASS scenery batches: TRI1/TRI4 indices, degenerate padding, content mutation/eviction, 32-command/128-triangle bounds and cross-frame reuse");
}
