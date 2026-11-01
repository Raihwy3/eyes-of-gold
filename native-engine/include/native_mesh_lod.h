#ifndef EOG_NATIVE_MESH_LOD_H
#define EOG_NATIVE_MESH_LOD_H
#include "native_scenery_batches.h"
#include <cmath>
#include <algorithm>
/* Collapse to existing vertices only. Open chunk boundaries, UV/normal seams,
 * manifold links, triangle orientation and non-duplicate faces are protected. */
struct NativeMeshLod {
    using Triangle=NativeSceneryBatches::Triangle;
    struct Input {int16_t xyz[3];uint16_t flag;int16_t uv[2];uint8_t rgba[4];};
    struct Entry {uint32_t hash;unsigned count[4];Triangle triangles[4][128];Input vertices[128];};
    Entry entries[64]{};unsigned builds=0,hits=0,deferred=0;
    void begin_frame(){builds=hits=deferred=0;}
    static bool has(const Triangle &t,unsigned v){return t.a==v || t.b==v || t.c==v;}
    static void normal(const Triangle &t,const Input *v,float *n) {
        float a[3],b[3];for(unsigned k=0;k<3;k++){a[k]=float(v[t.b].xyz[k])-v[t.a].xyz[k];b[k]=float(v[t.c].xyz[k])-v[t.a].xyz[k];}
        n[0]=a[1]*b[2]-a[2]*b[1];n[1]=a[2]*b[0]-a[0]*b[2];n[2]=a[0]*b[1]-a[1]*b[0];
    }
    static unsigned edge_count(const Triangle *tri,unsigned count,unsigned a,unsigned b) {
        unsigned n=0;for(unsigned i=0;i<count;i++)n+=has(tri[i],a)&&has(tri[i],b);return n;
    }
    static void locks(const Triangle *tri,unsigned count,const Input *v,bool *locked) {
        for(unsigned i=0;i<count;i++) {
            unsigned p[3]={tri[i].a,tri[i].b,tri[i].c};
            for(unsigned j=0;j<3;j++)if(edge_count(tri,count,p[j],p[(j+1)%3])!=2)locked[p[j]]=locked[p[(j+1)%3]]=true;
        }
        bool used[128]={};for(unsigned i=0;i<count;i++)used[tri[i].a]=used[tri[i].b]=used[tri[i].c]=true;
        for(unsigned a=0;a<128;a++)if(used[a])for(unsigned b=a+1;b<128;b++)if(used[b] && !memcmp(v[a].xyz,v[b].xyz,6)) {
            // Duplicate positions across an attribute seam cannot be joined.
            if(memcmp(v[a].uv,v[b].uv,4) || memcmp(v[a].rgba,v[b].rgba,4))locked[a]=locked[b]=true;
        }
    }
    static Triangle remap(Triangle t,unsigned a,unsigned b) {
        if(t.a==a)t.a=b;if(t.b==a)t.b=b;if(t.c==a)t.c=b;return t;
    }
    static bool valid(const Triangle *tri,unsigned count,const Input *v,unsigned a,unsigned b,float error2,float turn) {
        bool na[128]={},nb[128]={};unsigned common=0;
        for(unsigned i=0;i<count;i++) {
            if(has(tri[i],a))na[tri[i].a]=na[tri[i].b]=na[tri[i].c]=true;
            if(has(tri[i],b))nb[tri[i].a]=nb[tri[i].b]=nb[tri[i].c]=true;
        }
        for(unsigned i=0;i<128;i++)if(i!=a && i!=b && na[i] && nb[i])common++;
        if(common!=2 || edge_count(tri,count,a,b)!=2)return false;
        unsigned n=0;
        for(unsigned i=0;i<count;i++) {
            auto old=tri[i],t=remap(old,a,b);
            if(t.a==t.b || t.a==t.c || t.b==t.c)continue;
            if(has(old,a)) {
                float x[3],y[3];normal(old,v,x);normal(t,v,y);
                double xx=0,yy=0,xy=0,d=0;
                for(unsigned k=0;k<3;k++){xx+=double(x[k])*x[k];yy+=double(y[k])*y[k];xy+=double(x[k])*y[k];d+=(float(v[b].xyz[k])-v[a].xyz[k])*x[k];}
                if(!(xx>0 && yy>0 && xy>0) || xy*xy<turn*turn*xx*yy || d*d>error2*xx)return false;
            }
            // A tetrahedron must not collapse into duplicate opposite faces.
            if(has(old,a))for(unsigned j=0;j<count;j++)if(j!=i) {
                auto other=remap(tri[j],a,b);
                if(other.a!=other.b && other.a!=other.c && other.b!=other.c && has(other,t.a)&&has(other,t.b)&&has(other,t.c))return false;
            }
            n++;
        }
        return n>=2 && n<count;
    }
    static unsigned simplify(const Triangle *source,unsigned count,const Input *v,Triangle *out,unsigned tier,unsigned *remaps=nullptr,bool planar=false) {
        if(remaps)for(unsigned i=0;i<128;i++)remaps[i]=i;
        memcpy(out,source,count*sizeof(Triangle));if(count<4)return count;
        bool locked[128]={},used[128]={};locks(source,count,v,locked);
        for(unsigned i=0;i<count;i++)used[source[i].a]=used[source[i].b]=used[source[i].c]=true;
        float lo[3]={1e9f,1e9f,1e9f},hi[3]={-1e9f,-1e9f,-1e9f},diam2=0;
        for(unsigned i=0;i<128;i++)if(used[i])for(unsigned k=0;k<3;k++){lo[k]=std::min(lo[k],float(v[i].xyz[k]));hi[k]=std::max(hi[k],float(v[i].xyz[k]));}
        for(unsigned k=0;k<3;k++)diam2+=(hi[k]-lo[k])*(hi[k]-lo[k]);
        const float error2=planar?0.f:diam2*(tier==1?.0004f:tier==2?.0025f:.0064f),edge2=diam2*(tier==1?.2f:tier==2?.5f:.8f);
        const unsigned target=std::max(2u,count*(tier<3?3u:2u)/4u);unsigned budget=512;
        while(count>target && budget) {
            bool changed=false;
            for(unsigned i=0;i<count && !changed && budget;i++)for(unsigned e=0;e<6 && !changed && budget;e++) {
                unsigned p[3]={out[i].a,out[i].b,out[i].c};unsigned a=p[e%3],b=p[(e%3+(e<3?1:2))%3];
                if(locked[a])continue;
                float d2=0;for(unsigned k=0;k<3;k++){float d=float(v[a].xyz[k])-v[b].xyz[k];d2+=d*d;}
                if(d2>edge2)continue;
                // Strong discontinuities in baked colour or normals stay.
                bool seam=false;for(unsigned k=0;k<4;k++)if(std::abs(int(v[a].rgba[k])-int(v[b].rgba[k]))>(tier==1?24:tier==2?48:64))seam=true;
                if(seam)continue;
                --budget;if(!valid(out,count,v,a,b,error2,planar?.999999f:tier==1?.96f:tier==2?.88f:.8f))continue;
                unsigned n=0;for(unsigned j=0;j<count;j++){auto t=remap(out[j],a,b);if(t.a!=t.b && t.a!=t.c && t.b!=t.c)out[n++]=t;}
                if(remaps)for(unsigned j=0;j<128;j++)if(remaps[j]==a)remaps[j]=b;
                count=n;changed=true;
            }
            if(!changed)break;
        }
        return count;
    }
    const Entry *get(const Triangle *tri,unsigned count,const Input *vertices,const bool *available,bool planar=false) {
        bool used[128]={};for(unsigned i=0;i<count;i++)used[tri[i].a]=used[tri[i].b]=used[tri[i].c]=true;
        uint32_t h=planar?0x9e3779b9u:2166136261u;
        for(unsigned i=0;i<count;i++){h=(h^tri[i].a)*16777619u;h=(h^tri[i].b)*16777619u;h=(h^tri[i].c)*16777619u;}
        for(unsigned i=0;i<128;i++)if(used[i]) {
            if(!available[i])return nullptr;
            for(unsigned k=0;k<4;k++){uint32_t word;memcpy(&word,reinterpret_cast<const uint8_t *>(vertices+i)+4*k,4);h=(h^word)*16777619u;}
        }
        h^=h>>16;auto &entry=entries[h%64];bool same=entry.hash==h && entry.count[0]==count && !memcmp(entry.triangles[0],tri,count*sizeof(Triangle));
        if(same)for(unsigned i=0;i<128;i++)if(used[i] && memcmp(entry.vertices+i,vertices+i,sizeof(Input))){same=false;break;}
        if(same){hits++;return &entry;}
        // Compile at most one distinct mesh per frame; render originals while busy.
        if(builds){deferred++;return nullptr;}
        builds++;entry.hash=h;entry.count[0]=count;memcpy(entry.triangles[0],tri,count*sizeof(Triangle));memcpy(entry.vertices,vertices,128*sizeof(Input));
        entry.count[1]=simplify(tri,count,vertices,entry.triangles[1],1,nullptr,planar);
        entry.count[2]=simplify(entry.triangles[1],entry.count[1],vertices,entry.triangles[2],2,nullptr,planar);
        entry.count[3]=simplify(entry.triangles[2],entry.count[2],vertices,entry.triangles[3],3,nullptr,planar);
        return &entry;
    }
};
static_assert(sizeof(NativeMeshLod::Input)==16,"Native GBI vertex layout");
static_assert(sizeof(NativeMeshLod)<=256*1024,"Mesh LOD cache budget");
#endif
