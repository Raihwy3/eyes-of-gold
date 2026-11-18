"""GPU model transforms/culling for scoped opaque world geometry.

Retain CPU Z/W for original fog/LOD, and lazy XY for exact special-case
fallback. Never change the UI/viewmodel projection or vertex attributes.
"""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    replace('static bool native_scenery_active;','')
    replace('static unsigned native_scenery_kind;','')
    replace('struct LoadedVertex {','''#include "native_gpu_world.h"
#include "native_gpu_bounds.h"
extern "C" int native_pica_gpu_world(const NativeGpuWorld *) __attribute__((weak));
extern "C" int native_pica_gpu_world_supported(void) __attribute__((weak));
extern "C" int native_pica_retained_supported(void) __attribute__((weak));
static bool native_gpu_world_active;
static bool native_scenery_active;
static unsigned native_scenery_kind;
static NativeGpuWorld native_gpu_current;
static unsigned native_gpu_matrix_tag;
static float native_gpu_matrix_rows[4][4];
static float native_gpu_matrices[512][4][4];
static unsigned native_gpu_world_triangles,native_gpu_world_loads,native_gpu_world_resolves;
static unsigned native_gpu_bounds_tested,native_gpu_bounds_rejected;
struct NativeGpuVertex {
    float xyz[3];
    unsigned matrix_tag;bool valid,deferred;
};
static NativeGpuVertex native_gpu_vertices[MAX_VERTICES];
static bool native_reference_background;
extern "C" void native_graphics_reference_background(int enabled){native_reference_background=enabled!=0;}
static bool native_gpu_world_available() {
    if(native_scenery_kind==6)return false; // Faded actors preserve the CPU transparency path.
    if(native_reference_background && native_scenery_kind==1)return false;
    static bool disabled=getenv("EOG_DISABLE_GPU_WORLD")!=nullptr;
    static bool wide=getenv("EOG_ENABLE_GPU_WORLD")!=nullptr;
    bool retained=native_pica_retained_supported && native_pica_retained_supported();
    bool enabled=!disabled && (wide || retained || native_scenery_kind==3 || native_scenery_kind==4);
    return enabled && native_pica_gpu_world && native_pica_gpu_world_supported && native_pica_gpu_world_supported();
}
struct LoadedVertex {''')
    pos=text.index('static void gfx_sp_vertex(size_t')
    text=text[:pos]+'''static void native_gpu_resolve(unsigned index) {
    if(index>=MAX_VERTICES)return;
    auto &g=native_gpu_vertices[index];
    if(!g.deferred)return;
    auto &v=rsp.loaded_vertices[index];
    const auto &rows=native_gpu_matrices[g.matrix_tag];
    v.x=g.xyz[0]*rows[0][0]+g.xyz[1]*rows[0][1]+g.xyz[2]*rows[0][2]+rows[0][3];
    v.y=g.xyz[0]*rows[1][0]+g.xyz[1]*rows[1][1]+g.xyz[2]*rows[1][2]+rows[1][3];
    v.clip_rej &= ~15;
    if(v.x<-v.w)v.clip_rej|=1;if(v.x>v.w)v.clip_rej|=2;
    if(v.y<-v.w)v.clip_rej|=4;if(v.y>v.w)v.clip_rej|=8;
    g.deferred=false;
    if(native_rsp_profile::enabled)native_gpu_world_resolves++;
}
'''+text[pos:]
    replace('    const size_t d75_di0 = dest_index; /* D75 probe: loop below mutates dest_index */', '''    bool gpu_load=native_scenery_active && native_gpu_world_available() && !tri_dbg() &&
        !(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE|G_NO_CLIPPING_EXT));
    float gpu_rows[4][4];
    if(gpu_load)for(unsigned col=0;col<4;col++)for(unsigned row=0;row<4;row++)
        gpu_rows[row][col]=row==0?gfx_adjust_x_for_aspect_ratio(rsp.MP_matrix[col][0],rsp.MP_matrix[col][3]):rsp.MP_matrix[col][row];
    if(gpu_load && (!native_gpu_matrix_tag || memcmp(gpu_rows,native_gpu_matrix_rows,sizeof gpu_rows))) {
        if(native_gpu_matrix_tag+1<512) {
            memcpy(native_gpu_matrix_rows,gpu_rows,sizeof gpu_rows);native_gpu_matrix_tag++;
            memcpy(native_gpu_matrices[native_gpu_matrix_tag],gpu_rows,sizeof gpu_rows);
        }else gpu_load=false;
    }
    const size_t d75_di0 = dest_index; /* D75 probe: loop below mutates dest_index */''')
    start=text.index('        float x = v->v.ob[0]');end=text.index('\n        short U =',start)
    old=text[start:end]
    # Only XY is deferred. Z/W remains exact for distance fog, LOD and shade.
    text=text[:start]+'''        auto &gpu=native_gpu_vertices[dest_index];
        gpu.valid=gpu_load;
        gpu.deferred=gpu.valid;
        if(gpu.valid) {
            for(unsigned axis=0;axis<3;axis++)gpu.xyz[axis]=v->v.ob[axis];
            gpu.matrix_tag=native_gpu_matrix_tag;
            if(native_rsp_profile::enabled)native_gpu_world_loads++;
        }
'''+old.replace('float x = ','float x = gpu.valid?0.f:').replace('float y = ','float y = gpu.valid?0.f:').replace('        x = gfx_adjust_x_for_aspect_ratio(x, w);','        if(!gpu.valid)x = gfx_adjust_x_for_aspect_ratio(x, w);')+text[end:]
    replace('static void gfx_sp_vertex_float(size_t n_vertices, size_t dest_index, const PortFloatVtx* vertices) {','''static void gfx_sp_vertex_float(size_t n_vertices, size_t dest_index, const PortFloatVtx* vertices) {
    for(size_t i=dest_index;i<MAX_VERTICES && i<dest_index+n_vertices;i++)native_gpu_vertices[i].valid=native_gpu_vertices[i].deferred=false;''')
    # Reference path needs the original clip coordinates, including mixed matrices.
    replace('static void gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {','''static void gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    if(native_gpu_world_active)native_set_gpu_world(nullptr);
    native_gpu_resolve(vtx1_idx);native_gpu_resolve(vtx2_idx);native_gpu_resolve(vtx3_idx);''')
    # No altered packed-vertex cache entries leak across GPU/reference mode.
    pos=text.index('static void native_set_texture_lod(unsigned tier)')
    text=text[:pos]+'''static void native_set_gpu_world(const NativeGpuWorld *world) {
    if(!world && !native_gpu_world_active)return;
    if(world && native_gpu_world_active && !memcmp(world,&native_gpu_current,sizeof *world))return;
    gfx_flush();
    if(world)native_gpu_current=*world;
    if(native_pica_gpu_world)native_pica_gpu_world(world);
    native_gpu_world_active=world!=nullptr;
    native_invalidate_packed_vertices();
}
'''+text[pos:]
    replace('        buf_vbo[buf_vbo_len++] = vi->x;\n        buf_vbo[buf_vbo_len++] = clip_parameters.invert_y ? -vi->y : vi->y;\n        buf_vbo[buf_vbo_len++] = z;\n        buf_vbo[buf_vbo_len++] = w;','''        if(native_gpu_world_active) {
            const auto &g=native_gpu_vertices[vi-rsp.loaded_vertices];
            buf_vbo[buf_vbo_len++]=g.xyz[0];buf_vbo[buf_vbo_len++]=g.xyz[1];
            buf_vbo[buf_vbo_len++]=g.xyz[2];buf_vbo[buf_vbo_len++]=1.f;
        }else {
            buf_vbo[buf_vbo_len++] = vi->x;
            buf_vbo[buf_vbo_len++] = clip_parameters.invert_y ? -vi->y : vi->y;
            buf_vbo[buf_vbo_len++] = z;
            buf_vbo[buf_vbo_len++] = w;
        }''')
    # LOD visibility must be conservative until XY is resolved.
    replace('bool hidden=(a.clip_rej & b.clip_rej & c.clip_rej)!=0;','bool hidden=(a.clip_rej & b.clip_rej & c.clip_rej & ~15)!=0;')
    # GPU block before existing per-triangle batch loop.
    needle='    for(unsigned i=0;i<triangle_count;i++) {'
    replace(needle,'''    bool gpu=native_gpu_world_available() && triangle_count &&
        (rdp.other_mode_l & Z_UPD) && !(rdp.other_mode_l & CVG_X_ALPHA) &&
        (rdp.other_mode_l & ZMODE_DEC)!=ZMODE_DEC && !(rdp.other_mode_l & G_ZS_PRIM) &&
        !(rsp.geometry_mode & G_TEXTURE_GEN);
    NativeGpuWorld world={};
    float bounds_lo[3]={1.e30f,1.e30f,1.e30f},bounds_hi[3]={-1.e30f,-1.e30f,-1.e30f};
    bool bounds_seen[MAX_VERTICES]={};
    if(gpu) {
        const auto &first=native_gpu_vertices[triangles[0].a];
        gpu=first.valid;if(first.valid)memcpy(world.rows,native_gpu_matrices[first.matrix_tag],sizeof world.rows);
        for(unsigned i=0;gpu && i<triangle_count;i++) {
            const auto &t=triangles[i];unsigned indices[]={t.a,t.b,t.c};
            for(unsigned index:indices) {
                const auto &v=native_gpu_vertices[index];
                if(!v.valid || v.matrix_tag!=first.matrix_tag)gpu=false;
                if(!bounds_seen[index]) {
                    bounds_seen[index]=true;
                    for(unsigned axis=0;axis<3;axis++) {
                        bounds_lo[axis]=std::min(bounds_lo[axis],v.xyz[axis]);
                        bounds_hi[axis]=std::max(bounds_hi[axis],v.xyz[axis]);
                    }
                }
            }
        }
    }
    if(gpu) {
        if(native_rsp_profile::enabled)native_gpu_bounds_tested++;
        if(native_gpu_bounds_outside(world.rows,bounds_lo,bounds_hi)) {
            if(native_rsp_profile::enabled)native_gpu_bounds_rejected++;
            return count;
        }
        native_prepare_triangles(false);
        // Screen-space noise and nonstandard depth conventions keep the reference.
        gpu=!(native_triangle_setup.comb->shader_id1 & 8) &&
            !native_triangle_setup.clip_parameters.z_is_from_0_to_1;
    }
    if(gpu) {
        world.cull=(rsp.geometry_mode&G_CULL_BOTH)==G_CULL_FRONT?1:
                   (rsp.geometry_mode&G_CULL_BOTH)==G_CULL_BACK?2:0;
        if((rsp.geometry_mode&G_CULL_BOTH)==G_CULL_BOTH)return count;
        if(native_triangle_setup.clip_parameters.invert_y) {
            for(float &v:world.rows[1])v=-v;
            if(world.cull)world.cull=3-world.cull;
        }
        bool active=false;
        for(unsigned i=0;i<triangle_count;i++) {
            const auto &t=triangles[i];
            auto *a=&rsp.loaded_vertices[t.a],*b=&rsp.loaded_vertices[t.b],*c=&rsp.loaded_vertices[t.c];
            // Near-plane interpolation must use the reference clipper even
            // with View distance=Original (no custom distance-fog scope).
            bool boundary=a->w<=0 || b->w<=0 || c->w<=0 ||
                a->z+a->w<=1e-4f || b->z+b->w<=1e-4f || c->z+c->w<=1e-4f;
            bool hidden=false;
            if(native_distance_fog_active) {
                float end=native_distance_fog.end_ndc;
                float da=end*a->w-a->z,db=end*b->w-b->z,dc=end*c->w-c->z;
                hidden=da<=0 && db<=0 && dc<=0;
                boundary=boundary || da<=0 || db<=0 || dc<=0;
            }
            if(native_baked_packet) {
                const auto &ga=native_gpu_vertices[t.a],&gb=native_gpu_vertices[t.b],&gc=native_gpu_vertices[t.c];
                if(!memcmp(ga.xyz,gb.xyz,sizeof ga.xyz) || !memcmp(ga.xyz,gc.xyz,sizeof ga.xyz) || !memcmp(gb.xyz,gc.xyz,sizeof ga.xyz)) {
                    if(native_rsp_profile::enabled){native_baked_removed++;if(native_mesh_output)native_mesh_output--;}
                    continue;
                }
            }
            if(hidden)continue;
            if(boundary) {
                submit();native_set_gpu_world(nullptr);active=false;
                gfx_sp_tri1(t.a,t.b,t.c,false);prepared=false;
                if(native_rsp_profile::enabled)native_batch_fallback++;
                continue;
            }
            if(!active) {
                if(!prepared){native_prepare_triangles(false);prepared=true;}
                native_set_gpu_world(&world);active=true;
            }
            emit[pending*3]=a;emit[pending*3+1]=b;emit[pending*3+2]=c;pending++;
            if(native_rsp_profile::enabled){native_gpu_world_triangles++;native_batch_triangles++;if(native_scenery_kind==2 || native_scenery_kind==4)native_object_fast++;}
        }
        submit();
        return count;
    }
    native_set_gpu_world(nullptr);
    for(unsigned i=0;i<triangle_count;i++) {
        native_gpu_resolve(triangles[i].a);native_gpu_resolve(triangles[i].b);native_gpu_resolve(triangles[i].c);''')
    replace('    gfx_rapi->start_frame();','''    gfx_rapi->start_frame();
    native_gpu_matrix_tag=0;native_gpu_world_active=false;native_gpu_world_triangles=native_gpu_world_loads=native_gpu_world_resolves=0;
    native_gpu_bounds_tested=native_gpu_bounds_rejected=0;native_cull_precise=0;
    memset(native_gpu_vertices,0,sizeof native_gpu_vertices);''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"GPU world: triangles=%u deferredXY=%u cpuResolved=%u",native_gpu_world_triangles,native_gpu_world_loads,native_gpu_world_resolves);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"GPU batch visibility: tested=%u rejectedBeforeMaterial=%u",native_gpu_bounds_tested,native_gpu_bounds_rejected);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"World winding: preciseFallbacks=%u",native_cull_precise);
'''+needle)
    return text
