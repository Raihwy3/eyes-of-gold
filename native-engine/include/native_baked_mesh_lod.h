#ifndef EOG_NATIVE_BAKED_MESH_LOD_H
#define EOG_NATIVE_BAKED_MESH_LOD_H
#include "native_mesh_lod.h"
#include <cstdlib>
#ifdef __3DS__
#include "native_derived.h"
#else
#include "native_bg_lods.h"
#endif
/* Immutable, content-validated lookup. No reducer, cache eviction or mesh
 * generation is run on the console; unknown/dynamic meshes keep originals. */
struct NativeBakedMeshLod {
 unsigned builds=0,hits=0,deferred=0,misses=0;
#ifndef __3DS__
 NativeMeshLod synthetic;
#endif
 void begin_frame(){builds=hits=deferred=misses=0;
#ifndef __3DS__
 synthetic.begin_frame();
#endif
 }
 const NativeMeshLod::Entry *get(const NativeMeshLod::Triangle *tri,unsigned count,
 const NativeMeshLod::Input *vertices,const bool *available,bool planar=false) {
#ifndef __3DS__
   // Synthetic safety fixtures exercise the reducer; production only looks up.
   if(getenv("EOG_TEST_GENERATED_LOD")) {
     auto *e=synthetic.get(tri,count,vertices,available,planar);
     builds=synthetic.builds;hits=synthetic.hits;deferred=synthetic.deferred;return e;
   }
#endif
   if(!planar){misses++;return nullptr;}
   bool used[128]={};uint32_t h=0x9e3779b9u;
   for(unsigned i=0;i<count;i++)for(unsigned v:{tri[i].a,tri[i].b,tri[i].c}) {
     if(v>=128 || !available[v])return nullptr;
     used[v]=true;h=(h^v)*16777619u;
   }
   for(unsigned i=0;i<128;i++)if(used[i])for(unsigned k=0;k<4;k++) {
     uint32_t word;memcpy(&word,reinterpret_cast<const uint8_t *>(vertices+i)+4*k,4);h=(h^word)*16777619u;
   }
   h^=h>>16;unsigned lo=0,hi=native_bg_lod_count;
   while(lo<hi){unsigned mid=(lo+hi)/2;if(native_bg_lods[mid].hash<h)lo=mid+1;else hi=mid;}
   for(unsigned j=lo;j<native_bg_lod_count && native_bg_lods[j].hash==h;j++) {
     const auto &e=native_bg_lods[j];if(e.count[0]!=count || memcmp(e.triangles[0],tri,count*sizeof *tri))continue;
     bool same=true;for(unsigned i=0;i<128;i++)if(used[i] && memcmp(e.vertices+i,vertices+i,sizeof *vertices)){same=false;break;}
     if(same){hits++;return &e;}
   }
   misses++;return nullptr;
 }
};
#endif
