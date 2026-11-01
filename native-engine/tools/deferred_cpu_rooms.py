"""Skip provably hidden CPU room blocks before exact vertex transforms/fog.

Visible output uses original load-time matrix/aspect/fog expressions, never
GPU model-space transforms or hardware winding/index rejection.
"""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    needle='static void native_bg_finalize(unsigned index) {'
    helper=r'''#include "native_cpu_room_bounds.h"
struct NativeCpuRoomState {
    float mp[4][4],offset,scale,aspect,mul,fog_offset;
    bool framebuffer,custom,vertex_fog,authored;unsigned alpha;
    NativeDistanceFog fog;
};
struct NativeCpuRoomVertex {float xyz[3];unsigned tag;bool pending;};
static NativeCpuRoomState native_cpu_room_states[256];
static NativeCpuRoomVertex native_cpu_room_vertices[MAX_VERTICES];
static unsigned native_cpu_room_tag,native_cpu_room_pending;
static unsigned native_cpu_room_loaded,native_cpu_room_resolved,native_cpu_room_tested,native_cpu_room_rejected;
static void native_cpu_room_clear(unsigned index) {
    auto &v=native_cpu_room_vertices[index];
    if(v.pending){v.pending=false;native_cpu_room_pending--;}
}
static unsigned native_cpu_room_state() {
    NativeCpuRoomState state;memset(&state,0,sizeof state);memcpy(state.mp,rsp.MP_matrix,sizeof state.mp);
    state.offset=rsp.aspect_ofs;state.scale=rsp.aspect_scale;state.aspect=gfx_current_dimensions.aspect_ratio;
    state.framebuffer=fbActive;state.custom=native_distance_fog_active;
    state.vertex_fog=gfx_fog_vertex;state.authored=(rsp.geometry_mode&G_FOG)!=0;
    state.mul=rsp.fog_mul;state.fog_offset=rsp.fog_offset;state.alpha=rdp.fog_color.a;
    if(state.custom)state.fog=native_distance_fog;
    if(native_cpu_room_tag && !memcmp(&state,&native_cpu_room_states[native_cpu_room_tag],sizeof state))return native_cpu_room_tag;
    if(native_cpu_room_tag+1>=256)return 0;
    native_cpu_room_states[++native_cpu_room_tag]=state;return native_cpu_room_tag;
}
static void native_cpu_room_resolve(unsigned index) {
    if(index>=MAX_VERTICES)return;
    auto &p=native_cpu_room_vertices[index];if(!p.pending)return;
    const auto &s=native_cpu_room_states[p.tag];const auto &m=s.mp;const auto &a=p.xyz;
    auto &v=rsp.loaded_vertices[index];
    float x=a[0]*m[0][0]+a[1]*m[1][0]+a[2]*m[2][0]+m[3][0];
    float y=a[0]*m[0][1]+a[1]*m[1][1]+a[2]*m[2][1]+m[3][1];
    float z=a[0]*m[0][2]+a[1]*m[1][2]+a[2]*m[2][2]+m[3][2];
    float w=a[0]*m[0][3]+a[1]*m[1][3]+a[2]*m[2][3]+m[3][3];
    if(!s.framebuffer)x=(s.offset*w+x)*s.scale/s.aspect;
    v.x=x;v.y=y;v.z=z;v.w=w;v.clip_rej=0;
    if(x<-w)v.clip_rej|=1;if(x>w)v.clip_rej|=2;
    if(y<-w)v.clip_rej|=4;if(y>w)v.clip_rej|=8;if(z>w)v.clip_rej|=32;
    if(s.custom) {
        v.fog=native_distance_fog_factor(&s.fog,v.z,v.w);
        v.fog_n=s.vertex_fog?v.fog:v.fog*v.w;
    }else if(s.authored) {
        if(fabsf(w)<.001f)w=.001f;
        float inv=1.f/w;if(!s.vertex_fog && inv<0)inv=std::numeric_limits<int16_t>::max();
        v.fog=clampf(z*inv*s.mul+s.fog_offset,0.f,255.f);
        v.fog_n=s.vertex_fog?v.fog:v.z*s.mul+v.w*s.fog_offset;
    }else {v.fog=s.alpha;v.fog_n=s.vertex_fog?v.fog:v.fog*v.w;}
    native_cpu_room_clear(index);if(native_rsp_profile::enabled)native_cpu_room_resolved++;
}
static bool native_cpu_room_prepare(const NativeMeshLod::Triangle *triangles,unsigned count) {
    if(!native_cpu_room_pending)return false;
    bool seen[MAX_VERTICES]={},compatible=true;unsigned slots[MAX_VERTICES],n=0,tag=0;
    float lo[3]={1.e30f,1.e30f,1.e30f},hi[3]={-1.e30f,-1.e30f,-1.e30f};
    for(unsigned i=0;i<count;i++)for(unsigned j:{unsigned(triangles[i].a),unsigned(triangles[i].b),unsigned(triangles[i].c)}) {
        if(seen[j])continue;seen[j]=true;slots[n++]=j;
        const auto &p=native_cpu_room_vertices[j];
        if(!p.pending){compatible=false;continue;}
        if(tag && tag!=p.tag)compatible=false;tag=p.tag;
        for(unsigned k=0;k<3;k++){lo[k]=std::min(lo[k],p.xyz[k]);hi[k]=std::max(hi[k],p.xyz[k]);}
    }
    if(compatible && tag && native_scenery_kind==1) {
        const auto &s=native_cpu_room_states[tag];
        if(native_rsp_profile::enabled)native_cpu_room_tested++;
        float end=native_distance_fog_active?native_distance_fog.end_ndc:0;
        if(native_cpu_room_outside(s.mp,lo,hi,s.framebuffer,s.offset,s.scale,s.aspect,end)) {
            if(native_rsp_profile::enabled)native_cpu_room_rejected++;
            return true;
        }
    }
    for(unsigned i=0;i<n;i++)native_cpu_room_resolve(slots[i]);
    return false;
}
'''
    replace(needle,helper+needle)
    needle='static void native_gpu_resolve(unsigned index) {'
    replace(needle,needle+'\n    native_cpu_room_resolve(index);')
    needle='    const size_t d75_di0 = dest_index; /* D75 probe: loop below mutates dest_index */'
    replace(needle,'''    static const bool native_cpu_deferred=getenv("EOG_DISABLE_DEFERRED_CPU_ROOMS")==nullptr;
    unsigned cpu_room_tag=0;
    if(native_cpu_deferred && !gpu_load && native_scenery_active && native_scenery_kind==1 && !tri_dbg() &&
       !(rsp.geometry_mode&(G_LIGHTING|G_TEXTURE_GEN)) &&
       !(rsp.extra_geometry_mode&(NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE|G_NO_CLIPPING_EXT)))cpu_room_tag=native_cpu_room_state();
'''+needle)
    needle='        gpu.deferred=gpu.valid;'
    replace(needle,needle+'''
        native_cpu_room_clear(dest_index);
        if(cpu_room_tag) {
            auto &p=native_cpu_room_vertices[dest_index];p.pending=true;p.tag=cpu_room_tag;native_cpu_room_pending++;
            for(unsigned k=0;k<3;k++)p.xyz[k]=v->v.ob[k];
            d->x=d->y=d->z=d->w=d->fog_n=0;d->fog=0;d->clip_rej=0;
            d->u=(short)(v->v.tc[0]*rsp.texture_scaling_factor.s>>16);
            d->v=(short)(v->v.tc[1]*rsp.texture_scaling_factor.t>>16);
            memcpy(&d->color,v->v.cn,4);
            if(native_rsp_profile::enabled)native_cpu_room_loaded++;
            continue;
        }
''')
    needle='        const PortFloatVtx* v = &vertices[i];'
    replace(needle,needle+'\n        native_cpu_room_clear(dest_index);')
    needle='    unsigned native_batch_visibility=native_gpu_batch_visibility(entry.triangles,entry.count);'
    replace(needle,'    if(native_cpu_room_prepare(entry.triangles,entry.count))return count;\n'+needle)
    needle='    memset(native_gpu_vertices,0,sizeof native_gpu_vertices);'
    replace(needle,needle+'''
    memset(native_cpu_room_vertices,0,sizeof native_cpu_room_vertices);native_cpu_room_tag=native_cpu_room_pending=0;
    native_cpu_room_loaded=native_cpu_room_resolved=native_cpu_room_tested=native_cpu_room_rejected=0;
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"CPU room blocks: deferred=%u resolved=%u avoidedTransforms=%u tested=%u rejected=%u",native_cpu_room_loaded,native_cpu_room_resolved,native_cpu_room_loaded-native_cpu_room_resolved,native_cpu_room_tested,native_cpu_room_rejected);
'''+needle)
    return text
