"""Reject background batches before resolving their original depth/fog.

Only unlit, scoped room vertices qualify. Retain load-time fog state and
matrix ownership so partial loads and later CPU consumers stay exact.
"""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    replace('static NativeGpuVertex native_gpu_vertices[MAX_VERTICES];', '''static NativeGpuVertex native_gpu_vertices[MAX_VERTICES];
struct NativePendingBackground {
    bool pending,custom,vertex_fog,authored;
    float mul,offset;unsigned alpha;
    NativeDistanceFog fog;
};
static NativePendingBackground native_pending_background[MAX_VERTICES];
static unsigned native_bg_deferred,native_bg_resolved;
''')
    replace('static void native_gpu_resolve(unsigned index) {', '''static void native_bg_finalize(unsigned index) {
    if(index>=MAX_VERTICES)return;
    auto &p=native_pending_background[index];if(!p.pending)return;
    auto &g=native_gpu_vertices[index];auto &v=rsp.loaded_vertices[index];
    const auto &m=native_gpu_matrices[g.matrix_tag];
    v.z=g.xyz[0]*m[2][0]+g.xyz[1]*m[2][1]+g.xyz[2]*m[2][2]+m[2][3];
    v.w=g.xyz[0]*m[3][0]+g.xyz[1]*m[3][1]+g.xyz[2]*m[3][2]+m[3][3];
    v.clip_rej=v.z>v.w?32:0;
    if(p.custom) {
        v.fog=native_distance_fog_factor(&p.fog,v.z,v.w);
        v.fog_n=p.vertex_fog?v.fog:v.fog*v.w;
    }else if(p.authored) {
        float w=fabsf(v.w)<.001f?.001f:v.w,inv=1.f/w;
        if(!p.vertex_fog && inv<0)inv=std::numeric_limits<int16_t>::max();
        v.fog=clampf(v.z*inv*p.mul+p.offset,0.f,255.f);
        v.fog_n=p.vertex_fog?v.fog:v.z*p.mul+v.w*p.offset;
    }else {
        v.fog=p.alpha;v.fog_n=p.vertex_fog?v.fog:v.fog*v.w;
    }
    p.pending=false;if(native_rsp_profile::enabled)native_bg_resolved++;
}
static void native_gpu_resolve(unsigned index) {
    native_bg_finalize(index);''')
    replace('        gpu.valid=gpu_load;', '''        native_pending_background[dest_index].pending=false;
        gpu.valid=gpu_load;''')
    needle='        float x = gpu.valid?0.f:'
    at=text.index(needle)
    text=text[:at]+'''        static const bool native_defer_bg=getenv("EOG_DISABLE_DEFERRED_BG")==nullptr;
        if(native_defer_bg && gpu.valid && native_scenery_kind==1 && !(rsp.geometry_mode&(G_LIGHTING|G_TEXTURE_GEN))) {
            auto &pending=native_pending_background[dest_index];
            pending.pending=true;pending.custom=native_distance_fog_active;
            pending.vertex_fog=gfx_fog_vertex;pending.authored=(rsp.geometry_mode&G_FOG)!=0;
            pending.mul=rsp.fog_mul;pending.offset=rsp.fog_offset;pending.alpha=rdp.fog_color.a;
            if(pending.custom)pending.fog=native_distance_fog;
            d->x=d->y=d->z=d->w=d->fog_n=0;d->fog=0;d->clip_rej=0;
            d->u=(short)(v->v.tc[0]*rsp.texture_scaling_factor.s>>16);
            d->v=(short)(v->v.tc[1]*rsp.texture_scaling_factor.t>>16);
            memcpy(&d->color,v->v.cn,4);
            if(native_rsp_profile::enabled)native_bg_deferred++;
            continue;
        }
'''+text[at:]
    replace('    for(size_t i=dest_index;i<MAX_VERTICES && i<dest_index+n_vertices;i++)native_gpu_vertices[i].valid=native_gpu_vertices[i].deferred=false;',
      '    for(size_t i=dest_index;i<MAX_VERTICES && i<dest_index+n_vertices;i++){native_pending_background[i].pending=false;native_gpu_vertices[i].valid=native_gpu_vertices[i].deferred=false;}')
    needle='static unsigned native_run_scenery_batch(Gfx *cmd) {'
    helper='''// Return 0 for unknown/mixed matrices, 1 visible/intersecting, 2 outside.
static unsigned native_gpu_batch_visibility(const NativeMeshLod::Triangle *triangles,unsigned count) {
    if(!count || !native_gpu_world_available())return 0;
    unsigned tag=native_gpu_vertices[triangles[0].a].matrix_tag;
    float lo[3]={1.e30f,1.e30f,1.e30f},hi[3]={-1.e30f,-1.e30f,-1.e30f};bool seen[MAX_VERTICES]={};
    for(unsigned i=0;i<count;i++)for(unsigned index:{unsigned(triangles[i].a),unsigned(triangles[i].b),unsigned(triangles[i].c)}) {
        const auto &v=native_gpu_vertices[index];if(!v.valid || v.matrix_tag!=tag)return 0;
        if(!seen[index]) {
            seen[index]=true;
            for(unsigned axis=0;axis<3;axis++){lo[axis]=std::min(lo[axis],v.xyz[axis]);hi[axis]=std::max(hi[axis],v.xyz[axis]);}
        }
    }
    if(native_rsp_profile::enabled)native_gpu_bounds_tested++;
    if(native_gpu_bounds_outside(native_gpu_matrices[tag],lo,hi)) {
        if(native_rsp_profile::enabled)native_gpu_bounds_rejected++;
        return 2;
    }
    return 1;
}
'''
    replace(needle,helper+needle)
    replace('    unsigned tier=0;\n    static const bool native_disable_mesh_lod=', '''    if(native_gpu_batch_visibility(entry.triangles,entry.count)==2)return count;
    for(unsigned i=0;i<entry.count;i++) {
        const auto &t=entry.triangles[i];native_bg_finalize(t.a);native_bg_finalize(t.b);native_bg_finalize(t.c);
    }
    unsigned tier=0;
    static const bool native_disable_mesh_lod=''')
    replace('    float bounds_lo[3]={1.e30f,1.e30f,1.e30f},bounds_hi[3]={-1.e30f,-1.e30f,-1.e30f};\n    bool bounds_seen[MAX_VERTICES]={};','')
    replace('''                if(!bounds_seen[index]) {
                    bounds_seen[index]=true;
                    for(unsigned axis=0;axis<3;axis++) {
                        bounds_lo[axis]=std::min(bounds_lo[axis],v.xyz[axis]);
                        bounds_hi[axis]=std::max(bounds_hi[axis],v.xyz[axis]);
                    }
                }''','')
    replace('''        if(native_rsp_profile::enabled)native_gpu_bounds_tested++;
        if(native_gpu_bounds_outside(world.rows,bounds_lo,bounds_hi)) {
            if(native_rsp_profile::enabled)native_gpu_bounds_rejected++;
            return count;
        }
        native_prepare_triangles(false);''','        native_prepare_triangles(false);')
    replace('    memset(native_gpu_vertices,0,sizeof native_gpu_vertices);','''    memset(native_gpu_vertices,0,sizeof native_gpu_vertices);
    memset(native_pending_background,0,sizeof native_pending_background);native_bg_deferred=native_bg_resolved=0;''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Deferred background: loaded=%u resolved=%u avoidedDepthFog=%u",native_bg_deferred,native_bg_resolved,native_bg_deferred-native_bg_resolved);
'''+needle)
    return text
