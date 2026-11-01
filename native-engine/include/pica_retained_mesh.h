#ifndef EOG_PICA_RETAINED_MESH_H
#define EOG_PICA_RETAINED_MESH_H
#include "native_retained_index.h"
/* Producer-owned, content-proven tokens identify immutable model geometry.
 * Only dynamic fog/LOD and matrix selectors are patched. Never recycle a
 * buffer referenced by this GPU frame, including repeated model instances. */
class PicaRetainedMesh {
public:
  struct Key {uint64_t token;unsigned offset;PicaMeshCache::Key layout;PicaInputPlan plan;bool alpha_product;};
  struct Entry {
    Key key={};unsigned frame=0,vertices=0,count=0;bool valid=false;
    Vertex *gpu=nullptr;uint16_t *indices=nullptr;uint8_t corners[32]={};C3D_BufInfo buffer={};
  };
private:
  static constexpr unsigned capacity=512,vertices_per_entry=32,indices_per_entry=192;
  Vertex *pool=nullptr;uint16_t *elements=nullptr;bool failed=false;
  Entry entries[capacity];unsigned frame=1,builds=0;
  NativeRetainedIndex<capacity,64> index;
  size_t dirty_first=capacity*vertices_per_entry,dirty_last=0;
  bool indices_dirty=false;
  static unsigned hash(const Key &k) {
    uint64_t h=k.token^(uint64_t(k.offset)*0x9e3779b97f4a7c15ULL);
    h^=h>>33;return unsigned(h^(h>>32));
  }
  static bool same(const Key &a,const Key &b) {
    const auto &x=a.layout,&y=b.layout;
    return a.token==b.token && a.offset==b.offset && a.alpha_product==b.alpha_product &&
      x.shader==y.shader && x.flags==y.flags && x.count==y.count && x.stride==y.stride && x.fog==y.fog &&
      !memcmp(x.dimensions,y.dimensions,sizeof x.dimensions) &&
      pica_tev_same({0,0,a.plan,0,0},{0,0,b.plan,0,0});
  }
public:
  unsigned hits=0,misses=0,reused=0,built=0,busy=0;
  void begin_frame(){if(++frame==0){for(auto &e:entries)e.frame=0;frame=1;}hits=misses=reused=built=busy=builds=0;}
  void dirty(Entry &e) {
    size_t first=e.gpu-pool;dirty_first=std::min(dirty_first,first);dirty_last=std::max(dirty_last,first+e.vertices);
  }
  Entry *find(const Key &key,bool copied_to_frame=false) {
    int found=index.find(hash(key),[&](unsigned i) {
      const auto &e=entries[i];if(!e.valid || !same(e.key,key))return false;
      if(e.frame==frame && !copied_to_frame){busy++;return false;}return true;
    });
    if(found>=0){auto &e=entries[found];index.touch(found);e.frame=frame;hits++;reused+=e.vertices;return &e;}
    misses++;return nullptr;
  }
  Entry *store(const Key &key,const Vertex *data,unsigned vertices,const uint16_t *indices,unsigned first) {
    if(!vertices || vertices>vertices_per_entry || key.layout.count>indices_per_entry || builds>=16)return nullptr;
    if(!pool && !failed) {
      pool=static_cast<Vertex *>(linearAlloc(capacity*vertices_per_entry*sizeof(Vertex)));
      elements=static_cast<uint16_t *>(linearAlloc(capacity*indices_per_entry*sizeof(uint16_t)));
      if(!pool || !elements){if(pool)linearFree(pool);if(elements)linearFree(elements);pool=nullptr;elements=nullptr;failed=true;}
    }
    if(!pool)return nullptr;
    unsigned slot_index=index.victim();
    auto &e=entries[slot_index];if(e.frame==frame)return nullptr;
    e=Entry{};e.key=key;e.vertices=vertices;e.count=key.layout.count;e.frame=frame;e.valid=true;
    e.gpu=pool+slot_index*vertices_per_entry;e.indices=elements+slot_index*indices_per_entry;
    memcpy(e.gpu,data,vertices*sizeof(Vertex));bool seen[32]={};
    for(unsigned i=0;i<e.count;i++) {
      unsigned index=indices[i]-first;
      if(index>=vertices){e.valid=false;return nullptr;}
      e.indices[i]=index;if(!seen[index]){e.corners[index]=i;seen[index]=true;}
    }
    BufInfo_Init(&e.buffer);BufInfo_Add(&e.buffer,e.gpu,sizeof(Vertex),5,0x43210);
    index.install(slot_index,hash(key));
    dirty(e);indices_dirty=true;builds++;built+=vertices;return &e;
  }
  void flush() {
    if(pool && dirty_last>dirty_first)GSPGPU_FlushDataCache(pool+dirty_first,(dirty_last-dirty_first)*sizeof(Vertex));
    if(elements && indices_dirty)GSPGPU_FlushDataCache(elements,capacity*indices_per_entry*sizeof(uint16_t));
    dirty_first=capacity*vertices_per_entry;dirty_last=0;indices_dirty=false;
  }
  size_t bytes()const{return pool?capacity*(vertices_per_entry*sizeof(Vertex)+indices_per_entry*sizeof(uint16_t)):0;}
  bool available()const{return !failed;}
  void clear(){if(pool)linearFree(pool);if(elements)linearFree(elements);pool=nullptr;elements=nullptr;failed=false;for(auto &e:entries)e=Entry{};index.clear();dirty_first=capacity*vertices_per_entry;dirty_last=0;indices_dirty=false;}
};
#endif
