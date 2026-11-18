#ifndef EOG_PICA_STATIC_ATTRIBUTES_H
#define EOG_PICA_STATIC_ATTRIBUTES_H
#include <cstdint>
#include <cstring>
/* Camera-independent PICA attributes only. Exact context comparison and
 * producer-issued versions protect against collisions and slot recycling.
 * No resource pointers, GPU allocations or positions survive a lookup. */
template<class V> struct PicaStaticAttributes {
 struct Context {uint64_t shader;uint32_t flags,plan[7],dimensions[8],lod_rgb,lod_alpha;};
 struct Material {Context key;uint32_t token;};
 struct Entry {uint64_t identity;uint32_t context;V value;};
 Material materials[128]{};Entry entries[1024]{};
 uint32_t serial=0;unsigned hits=0,misses=0,cross_frame=0,frame=1;
 unsigned built_frame[1024]{};
 void begin_frame(){hits=misses=cross_frame=0;if(++frame==0){memset(built_frame,0,sizeof built_frame);frame=1;}}
 uint32_t context(const Context &key) {
   uint32_t h=2166136261u;auto p=reinterpret_cast<const unsigned char *>(&key);
   for(unsigned i=0;i<sizeof key;i++)h=(h^p[i])*16777619u;
   auto &m=materials[h%128];
   if(m.token && !memcmp(&m.key,&key,sizeof key))return m.token;
   if(++serial==0){memset(materials,0,sizeof materials);memset(entries,0,sizeof entries);serial=1;}
   m.key=key;m.token=serial;return serial;
 }
 bool read(uint64_t identity,uint32_t context,V &out) {
   auto &e=entries[identity&1023];
   if(identity && e.identity==identity && e.context==context) {
     out=e.value;hits++;if(built_frame[identity&1023]!=frame)cross_frame++;return true;
   }
   misses++;return false;
 }
 void write(uint64_t identity,uint32_t context,const V &value) {
   if(!identity)return;
   entries[identity&1023]={identity,context,value};built_frame[identity&1023]=frame;
 }
};
#endif
