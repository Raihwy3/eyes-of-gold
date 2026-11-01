#ifndef EOG_NATIVE_SCENERY_CACHE_H
#define EOG_NATIVE_SCENERY_CACHE_H
#include <cstdint>
#include <cstring>
/* Bounded content cache: no pointers to room allocations or GPU resources.
 * Context bytes must be fully initialized. Exact equality resolves hash
 * collisions; unique tokens prevent evicted contexts aliasing old entries. */
template<class Context> struct NativeSceneryCache {
    struct Material {Context value;uint32_t hash,token;};
    struct VertexKey {uint32_t context,u,v,colour;};
    struct Vertex {VertexKey key;unsigned count,built_frame;uint64_t identity;float attributes[36];};
    Material materials[128]{};
    Vertex vertices[1024]{};
    uint32_t next_token=0,current_hash=0;
    const Context *current_context=nullptr;
    uint64_t next_identity=0;
    unsigned hits=0,misses=0,cross_frame_hits=0,frame=0;
    void begin_frame(){if(++frame==0)frame=1;hits=misses=cross_frame_hits=0;}
    static uint32_t hash(const void *data,size_t bytes) {
        auto p=static_cast<const unsigned char *>(data);uint32_t h=2166136261u;
        while(bytes--)h=(h^*p++)*16777619u;
        return h;
    }
    uint32_t context(const Context &value) {
        uint32_t h=hash(&value,sizeof value);auto &entry=materials[h%128];
        current_hash=h;current_context=&entry.value;
        if(entry.token && entry.hash==h && !memcmp(&entry.value,&value,sizeof value))return entry.token;
        if(++next_token==0) {memset(materials,0,sizeof materials);memset(vertices,0,sizeof vertices);next_token=1;}
        entry.value=value;entry.hash=h;entry.token=next_token;return next_token;
    }
    static VertexKey key(uint32_t context,float u,float v,const void *colour) {
        VertexKey k{context,0,0,0};memcpy(&k.u,&u,4);memcpy(&k.v,&v,4);memcpy(&k.colour,colour,4);return k;
    }
    Vertex &slot(const VertexKey &k) {
        uint32_t h=k.context*0x9e3779b9u;
        h^=k.u+0x9e3779b9u+(h<<6)+(h>>2);
        h^=k.v+0x9e3779b9u+(h<<6)+(h>>2);
        h^=k.colour+0x9e3779b9u+(h<<6)+(h>>2);
        h^=h>>16;return vertices[h%1024];
    }
    bool read(const VertexKey &k,float *out,unsigned &count,uint64_t *identity=nullptr) {
        auto &entry=slot(k);
        if(entry.count && !memcmp(&entry.key,&k,sizeof k)) {
            if(identity)*identity=entry.identity;
            count=entry.count;memcpy(out,entry.attributes,count*sizeof(float));hits++;
            if(entry.built_frame!=frame)cross_frame_hits++;
            return true;
        }
        misses++;return false;
    }
    uint64_t write(const VertexKey &k,const float *data,unsigned count) {
        if(!count || count>36)return 0;
        auto &entry=slot(k);entry.identity=next_identity<(UINT64_MAX>>10)?(++next_identity<<10)|unsigned(&entry-vertices):0;entry.key=k;entry.count=count;entry.built_frame=frame;memcpy(entry.attributes,data,count*sizeof(float));return entry.identity;
    }
};
#endif
