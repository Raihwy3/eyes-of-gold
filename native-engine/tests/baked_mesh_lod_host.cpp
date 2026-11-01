#include "native_baked_mesh_lod.h"
// Host-only oracle storage behind the same dynamic-pointer ABI as the console.
#define native_bg_lods reference_bg_lods
#define native_bg_lod_count reference_bg_lod_count
#include "native_bg_lods.h"
#undef native_bg_lods
#undef native_bg_lod_count
NativeMeshLod::Entry *native_bg_lods=const_cast<NativeMeshLod::Entry *>(reference_bg_lods);
unsigned native_bg_lod_count=reference_bg_lod_count;
#include <cassert>
#include <cstdio>
#include <vector>
static_assert(sizeof(NativeBakedMeshLod)<128,"Console lookup must not instantiate the runtime reducer cache");
int main(int argc,char **argv) {
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);unsigned records=0;NativeBakedMeshLod lookup;
 while(true) {
   unsigned meta[5];if(fread(meta,4,5,f)!=5)break;
   NativeMeshLod::Input vertices[128];assert(fread(vertices,16,128,f)==128);
   std::vector<NativeMeshLod::Triangle> tri(meta[1]),mid(meta[2]),far(meta[3]),distant(meta[4]);
   assert(fread(tri.data(),3,tri.size(),f)==tri.size());assert(fread(mid.data(),3,mid.size(),f)==mid.size());assert(fread(far.data(),3,far.size(),f)==far.size());assert(fread(distant.data(),3,distant.size(),f)==distant.size());
   bool available[128];for(bool &a:available)a=true;
   for(unsigned frame=0;frame<250;frame++) {
     lookup.begin_frame();auto *e=lookup.get(tri.data(),tri.size(),vertices,available,true);
     assert(e && e->hash==meta[0] && e->count[1]==mid.size() && e->count[2]==far.size() && e->count[3]==distant.size());
     assert(!memcmp(e->triangles[1],mid.data(),mid.size()*3) && !memcmp(e->triangles[2],far.data(),far.size()*3) && !memcmp(e->triangles[3],distant.data(),distant.size()*3));
     assert(!lookup.builds && lookup.hits==1 && !lookup.deferred);
   }
   unsigned v=tri[0].a;available[v]=false;assert(!lookup.get(tri.data(),tri.size(),vertices,available,true));available[v]=true;
   auto original=vertices[v];vertices[v].xyz[0]^=1;assert(!lookup.get(tri.data(),tri.size(),vertices,available,true));vertices[v]=original;
   vertices[v].uv[0]^=1;assert(!lookup.get(tri.data(),tri.size(),vertices,available,true));vertices[v]=original;
   vertices[v].rgba[0]^=1;assert(!lookup.get(tri.data(),tri.size(),vertices,available,true));vertices[v]=original;
   assert(!lookup.get(tri.data(),tri.size(),vertices,available,false));records++;
 }
 assert(feof(f) && records==native_bg_lod_count && records>0);fclose(f);
 printf("PASS offline retail mesh LOD: %u reduced Dam runs, 250 lookups each without compilation/eviction, exact tiers and position/UV/colour/availability mutation rejection\n",records);
}
