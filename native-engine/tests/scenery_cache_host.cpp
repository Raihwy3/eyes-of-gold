#include "native_scenery_cache.h"
#include <cassert>
#include <cstdio>
struct Context {unsigned shader,tile,colour,filter;};
int main() {
    NativeSceneryCache<Context> cache;
    cache.begin_frame();
    Context a{1,2,3,4};unsigned char rgba[4]={1,2,3,4};
    auto token=cache.context(a);assert(token && cache.context(a)==token);
    assert(cache.current_context && !memcmp(cache.current_context,&a,sizeof a));
    assert(cache.current_hash==cache.hash(&a,sizeof a));
    auto key=decltype(cache)::key(token,4.f,8.f,rgba);
    float payload[36],out[36];for(unsigned i=0;i<36;i++)payload[i]=i*.25f;
    unsigned count=0;assert(!cache.read(key,out,count));
    cache.write(key,payload,36);assert(cache.read(key,out,count));
    assert(count==36 && !memcmp(out,payload,sizeof out));
    assert(!cache.cross_frame_hits);cache.begin_frame();
    assert(cache.read(key,out,count) && cache.cross_frame_hits==1);
    auto changed=key;changed.u++;assert(!cache.read(changed,out,count));
    changed=key;changed.v++;assert(!cache.read(changed,out,count));
    changed=key;changed.colour++;assert(!cache.read(changed,out,count));
    for(unsigned field=0;field<4;field++) {
        Context b=a;
        switch(field){case 0:b.shader++;break;case 1:b.tile++;break;case 2:b.colour++;break;default:b.filter++;break;}
        auto next=cache.context(b);assert(next!=token);
        auto k=key;k.context=next;assert(!cache.read(k,out,count));
    }
    // Force a direct-mapped collision: exact keys must reject stale bytes.
    changed=key;
    do{changed.colour++;}while(&cache.slot(changed)!=&cache.slot(key));
    cache.write(changed,payload,4);assert(!cache.read(key,out,count));
    assert(cache.read(changed,out,count) && count==4);
    cache.write(changed,payload,37);assert(cache.read(changed,out,count) && count==4);
    // Context eviction/reused material slots cannot reuse an older token.
    for(unsigned i=0;i<1000;i++){Context b{i+100,2,3,4};cache.context(b);}
    assert(cache.context(a)!=token);
    cache.next_token=0xffffffff;Context b{99999,99999,99999,99999};
    assert(cache.context(b)==1);assert(!cache.read(changed,out,count));
    auto fresh=key;fresh.context=cache.context(a);
    uint64_t first_id=cache.write(fresh,payload,4),read_id=0;
    assert(first_id && cache.read(fresh,out,count,&read_id) && first_id==read_id);
    assert(cache.write(fresh,payload,4)!=first_id);
    cache.next_identity=UINT64_MAX>>10;
    assert(cache.write(fresh,payload,4)==0); // permanently disable version reuse at exhaustion
    puts("PASS scenery cache: exact UV/colour/material keys, content collisions, eviction, token wrap, payload bounds");
}
