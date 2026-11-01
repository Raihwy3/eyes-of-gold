#ifndef EOG_PICA_MESH_CACHE_H
#define EOG_PICA_MESH_CACHE_H
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "pica_inputs.h"
#include "pica_fog.h"
#include "pica_texcoord.h"
/* Content-validated model-space buffers, not pointers into stage allocations.
 * FrameBegin must wait for the preceding frame before begin_frame(). Never
 * overwrite/evict a buffer already submitted in the current frame. */
class PicaMeshCache {
public:
  struct Key {uint64_t shader;uint32_t flags;unsigned count,stride,fog;unsigned dimensions[8];unsigned lod=~0u;bool lod_alpha=false;};
  struct Entry {
    bool valid=false,alpha_product=false;
    Key key={};uint32_t hash=0;unsigned frame=0;
    float *source=nullptr;Vertex *gpu=nullptr;C3D_BufInfo buffer={};
    PicaInputPlan inputs={};float fog_colour[3]={};size_t bytes=0;
  };
private:
  static constexpr unsigned capacity=256,ways=4;
  static constexpr unsigned page_vertices=16,page_count=512;
  static constexpr size_t pool_bytes=page_vertices*page_count*sizeof(Vertex);
  static constexpr size_t source_budget=1024*1024-pool_bytes;
  Vertex *pool=nullptr;bool pages[page_count]={},pool_failed=false;
  size_t dirty_first=pool_bytes,dirty_last=0;
  void dirty(const Vertex *p,size_t count) {
    size_t offset=(p-pool)*sizeof(Vertex);
    dirty_first=std::min(dirty_first,offset);dirty_last=std::max(dirty_last,offset+count*sizeof(Vertex));
  }
  int free_run(unsigned count)const {
    unsigned run=0;
    for(unsigned i=0;i<page_count;i++) {
      run=pages[i]?0:run+1;if(run==count)return i+1-count;
    }
    return -1;
  }
  Entry entries[capacity];size_t live=0;unsigned current_frame=1,builds=0;
  void release(Entry &e) {
    if(e.gpu) {
      unsigned first=(e.gpu-pool)/page_vertices,n=(e.key.count+page_vertices-1)/page_vertices;
      for(unsigned i=0;i<n;i++)pages[first+i]=false;
    }
    free(e.source);live-=e.bytes;e=Entry{};
  }
  static uint32_t hash(const Key &k,const float *data) {
    uint32_t h=uint32_t(k.shader)^uint32_t(k.shader>>32)^k.flags;
    auto mix=[&](uint32_t v){h=(h^v)*16777619u;};
    mix(k.count);mix(k.stride);mix(k.fog);mix(k.lod);mix(k.lod_alpha);for(unsigned v:k.dimensions)mix(v);
    for(unsigned i=0;i<k.count;i++)for(unsigned j=0;j<k.stride;j++) {
      if((k.fog!=~0u && j==k.fog+3) || (k.lod!=~0u && j>=k.lod && j<k.lod+3+k.lod_alpha))continue;
      uint32_t v;memcpy(&v,data+i*k.stride+j,4);mix(v);
    }
    return h^(h>>16);
  }
  static bool same(const Key &a,const Key &b) {
    return a.shader==b.shader && a.flags==b.flags && a.count==b.count &&
      a.stride==b.stride && a.fog==b.fog && a.lod==b.lod && a.lod_alpha==b.lod_alpha && !memcmp(a.dimensions,b.dimensions,sizeof a.dimensions);
  }
  static bool equal(const Entry &e,const float *data,bool include_fog) {
    if(include_fog || (e.key.fog==~0u && e.key.lod==~0u))return !memcmp(e.source,data,e.key.count*e.key.stride*4);
    for(unsigned i=0;i<e.key.count;i++) {
      const float *a=e.source+i*e.key.stride,*b=data+i*e.key.stride;
      unsigned start=0;
      for(unsigned j=0;j<=e.key.stride;j++) {
        bool dynamic=(e.key.fog!=~0u && j==e.key.fog+3) ||
          (e.key.lod!=~0u && j>=e.key.lod && j<e.key.lod+3+e.key.lod_alpha);
        if(dynamic || j==e.key.stride) {
          if(j>start && memcmp(a+start,b+start,(j-start)*4))return false;
          start=j+1;
        }
      }
    }
    return true;
  }
public:
  unsigned hits=0,misses=0,converted=0,reused=0,blocked=0;
  void begin_frame() {
    // Serial is independent of the renderer's wrapping frame counter.
    if(++current_frame==0){for(auto &e:entries)e.frame=0;current_frame=1;}
    hits=misses=converted=reused=blocked=builds=0;
  }
  Entry *find(const Key &key,const float *data,uint32_t &h) {
    h=hash(key,data);unsigned base=(h%(capacity/ways))*ways;
    for(unsigned n=0;n<ways;n++) {
      auto &e=entries[base+n];
      if(!e.valid || e.hash!=h || !same(e.key,key) || !equal(e,data,false))continue;
      if(e.frame==current_frame && !equal(e,data,true)){blocked++;break;}
      if(key.fog!=~0u || key.lod!=~0u) {
        for(unsigned i=0;i<key.count;i++)if(key.fog!=~0u && !std::isfinite(data[i*key.stride+key.fog+3])){misses++;return nullptr;}
        if(e.frame!=current_frame) {
          for(unsigned i=0;i<key.count;i++) {
            if(key.fog!=~0u) {
              float factor=data[i*key.stride+key.fog+3];
              e.gpu[i].uv[2][1]=pica_lookup_t(pica_ramp_coordinate(factor));
              e.source[i*key.stride+key.fog+3]=factor;
            }
            if(key.lod!=~0u) {
              e.gpu[i].uv[2][0]=pica_ramp_coordinate(data[i*key.stride+key.lod]);
              for(unsigned j=0;j<3u+key.lod_alpha;j++)e.source[i*key.stride+key.lod+j]=data[i*key.stride+key.lod+j];
            }
          }
          dirty(e.gpu,key.count);
        }
      }
      e.frame=current_frame;hits++;reused+=key.count;return &e;
    }
    misses++;return nullptr;
  }
  Entry *store(const Key &key,uint32_t h,const float *data,const Vertex *packed,
               const PicaInputPlan &plan,bool alpha_product,const float *fog) {
    converted+=key.count;
    size_t bytes=key.count*key.stride*4;
    unsigned needed=(key.count+page_vertices-1)/page_vertices;
    if(builds>=4 || bytes>source_budget || needed>page_count)return nullptr;
    if(!pool && !pool_failed){pool=static_cast<Vertex *>(linearAlloc(pool_bytes));pool_failed=!pool;}
    if(!pool)return nullptr;
    unsigned base=(h%(capacity/ways))*ways;Entry *slot=nullptr;
    for(unsigned n=0;n<ways;n++) {
      auto &e=entries[base+n];
      if(!e.valid){slot=&e;break;}
      if(e.frame!=current_frame && (!slot || e.frame<slot->frame))slot=&e;
    }
    if(!slot)return nullptr;
    // Recycling only previous-frame entries is safe after blocking FrameBegin.
    release(*slot);
    int first=free_run(needed);
    while(live+bytes>source_budget || first<0) {
      Entry *old=nullptr;
      for(auto &e:entries)if(&e!=slot && e.valid && e.frame!=current_frame && (!old || e.frame<old->frame))old=&e;
      if(!old)return nullptr;
      release(*old);first=free_run(needed);
    }
    auto *source=static_cast<float *>(malloc(bytes));
    if(!source)return nullptr;
    auto *gpu=pool+first*page_vertices;
    for(unsigned i=0;i<needed;i++)pages[first+i]=true;
    memcpy(source,data,key.count*key.stride*4);memcpy(gpu,packed,key.count*sizeof(Vertex));
    release(*slot);slot->key=key;slot->hash=h;slot->source=source;slot->gpu=gpu;
    slot->bytes=bytes;live+=bytes;slot->frame=current_frame;slot->valid=true;
    slot->inputs=plan;slot->alpha_product=alpha_product;memcpy(slot->fog_colour,fog,12);
    BufInfo_Init(&slot->buffer);BufInfo_Add(&slot->buffer,gpu,sizeof(Vertex),5,0x43210);
    dirty(gpu,key.count);builds++;return slot;
  }
  // As with the ordinary streaming VBO, CPU writes finish before FrameEnd
  // submits the queued GPU commands. One contiguous flush avoids per-mesh IPC.
  void flush() {
    if(pool && dirty_last>dirty_first)GSPGPU_FlushDataCache(reinterpret_cast<char *>(pool)+dirty_first,dirty_last-dirty_first);
    dirty_first=pool_bytes;dirty_last=0;
  }
  size_t bytes()const{return live+(pool?pool_bytes:0);}
  void clear(){for(auto &e:entries)release(e);if(pool)linearFree(pool);pool=nullptr;pool_failed=false;dirty_first=pool_bytes;dirty_last=0;}
};
#endif
