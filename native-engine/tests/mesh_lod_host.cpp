#include "native_mesh_lod.h"
#include "native_detail.h"
#include <cassert>
#include <cstdio>
static unsigned edge_count(const NativeMeshLod::Triangle *t,unsigned n,unsigned a,unsigned b){return NativeMeshLod::edge_count(t,n,a,b);}
int main(){
    using M=NativeMeshLod;M::Input v[128]={};bool available[128]={};M::Triangle tri[128],mid[128],far[128],distant[128];
    unsigned n=0;
    for(int y=0;y<5;y++)for(int x=0;x<5;x++){auto &p=v[y*5+x];p.xyz[0]=x*100;p.xyz[1]=y*100;p.uv[0]=x*64;p.uv[1]=y*64;memset(p.rgba,127,4);available[y*5+x]=true;}
    for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){
        unsigned a=y*5+x;tri[n++]={uint8_t(a),uint8_t(a+1),uint8_t(a+5)};tri[n++]={uint8_t(a+1),uint8_t(a+6),uint8_t(a+5)};
    }
    unsigned m=M::simplify(tri,n,v,mid,1),f=M::simplify(mid,m,v,far,2),d=M::simplify(far,f,v,distant,3);
    assert(m<n && f<m && d<f);
    for(unsigned a=0;a<25;a++)for(unsigned b=a+1;b<25;b++)if(edge_count(tri,n,a,b)==1){assert(edge_count(mid,m,a,b)==1);assert(edge_count(far,f,a,b)==1);assert(edge_count(distant,d,a,b)==1);}
    for(unsigned i=0;i<f;i++){float normal[3];M::normal(far[i],v,normal);assert(normal[2]>0);}
    for(unsigned i=0;i<d;i++){float normal[3];M::normal(distant[i],v,normal);assert(normal[2]>0);}
    M::Input warped[128];memcpy(warped,v,sizeof v);warped[12].xyz[2]=20;
    M::Triangle planar[128];unsigned kept=M::simplify(tri,n,warped,planar,2,nullptr,true);bool peak=false;
    for(unsigned i=0;i<kept;i++)peak=peak || M::has(planar[i],12);
    assert(peak); // Non-planar terrain must not flatten away from its decals.
    // A closed tetrahedron cannot collapse into duplicate opposing faces.
    M::Triangle tetra[4]={{0,1,2},{0,3,1},{1,3,2},{0,2,3}};M::Input tv[128]={};
    tv[1].xyz[0]=100;tv[2].xyz[1]=100;tv[3].xyz[2]=100;
    assert(M::simplify(tetra,4,tv,mid,2)==4);
    static M cache;cache.begin_frame();auto *e=cache.get(tri,n,v,available);assert(e && e->count[2]<e->count[1] && e->count[3]<e->count[2]);
    assert(cache.get(tri,n,v,available)==e && cache.hits==1);
    v[0].uv[0]++;assert(!cache.get(tri,n,v,available) && cache.deferred==1);
    cache.begin_frame();assert(cache.get(tri,n,v,available));available[0]=false;assert(!cache.get(tri,n,v,available));
    assert(native_detail_tier(599,1)==0 && native_detail_tier(600,1)==1 && native_detail_tier(1500,1)==2);
    assert(native_detail_tier(100000,0)==0 && native_detail_tier(3000,1)==3);
    printf("PASS three mesh LODs: planar grid %u -> %u -> %u -> %u, all chunk boundary edges retained, winding/manifold safety, immutable input keys and bounded compilation\n",n,m,f,d);
}
