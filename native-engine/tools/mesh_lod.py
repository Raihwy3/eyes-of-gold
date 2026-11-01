"""Distance tiers for bounded, boundary-locked generated mesh LODs."""
def apply(text):
    needle='struct LoadedVertex {';assert text.count(needle)==1
    text=text.replace(needle,'''#include "native_mesh_lod.h"
#include "native_baked_mesh_lod.h"
#include "native_lod_scope.h"
#include "native_effects.h"
#include "native_baked_lod.h"
#include "native_detail.h"
static NativeBakedMeshLod native_mesh_lod;
static NativeMeshLod::Input native_lod_inputs[MAX_VERTICES];
static bool native_lod_available[MAX_VERTICES];
static unsigned native_lod_matrix_tag=1,native_lod_matrix_tags[MAX_VERTICES];
static float native_lod_world_scale;
static int native_lod_strength;
static const NativeBakedLodPacket *native_baked_packet;
static unsigned native_baked_vertices,native_baked_changed,native_baked_removed;
static unsigned native_texture_tier,native_effect_texture_tier;
static void native_set_texture_lod(unsigned);
static unsigned native_mesh_tiers[4],native_mesh_input,native_mesh_output;
extern "C" void native_pica_texture_lod(unsigned) __attribute__((weak));
'''+needle)
    needle='// The reference triangle path and compiled batches share exact packing.'
    assert text.count(needle)==1
    text=text.replace(needle,'''static void native_set_texture_lod(unsigned tier) {
    if(tier==native_texture_tier)return;
    gfx_flush();if(native_pica_texture_lod)native_pica_texture_lod(tier);native_texture_tier=tier;
}
'''+needle)
    for needle in ('static void gfx_sp_matrix(uint8_t parameters, const int32_t* addr) {', 'static void gfx_sp_pop_matrix(uint32_t count) {'):
        assert text.count(needle)==1,needle
        text=text.replace(needle,needle+'''
    if(++native_lod_matrix_tag==0){native_lod_matrix_tag=1;memset(native_lod_available,0,sizeof native_lod_available);}
''')
    needle='static void native_prepare_triangles(bool is_rect) {'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
    if(!native_scenery_active || is_rect || (rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE)))native_set_texture_lod(0);
''')
    needle='        const Vtx* v = &vertices[i];';assert text.count(needle)==1
    text=text.replace(needle,needle+'''
        Vtx native_vertex_copy;
        if(native_baked_packet) {
            uintptr_t offset=(uintptr_t)v-(uintptr_t)native_baked_packet->base;
            if(offset%sizeof(Vtx)==0 && offset/sizeof(Vtx)<native_baked_packet->count) {
                native_vertex_copy=*v;
                for(unsigned axis=0;axis<3;axis++)native_vertex_copy.v.ob[axis]=native_baked_packet->xyz[(offset/sizeof(Vtx))*3+axis];
                if(native_rsp_profile::enabled){native_baked_vertices++;if(memcmp(v->v.ob,native_vertex_copy.v.ob,6))native_baked_changed++;}
                v=&native_vertex_copy;
            }
        }
        static_assert(sizeof(Vtx)==sizeof(NativeMeshLod::Input));
        if(native_lod_strength>0){memcpy(&native_lod_inputs[dest_index],v,sizeof(Vtx));native_lod_available[dest_index]=true;native_lod_matrix_tags[dest_index]=native_lod_matrix_tag;}else native_lod_available[dest_index]=false;
''')
    needle='static void gfx_sp_vertex_float(size_t n_vertices, size_t dest_index, const PortFloatVtx* vertices) {';assert text.count(needle)==1
    text=text.replace(needle,needle+'''
    for(size_t i=dest_index;i<MAX_VERTICES && i<dest_index+n_vertices;i++)native_lod_available[i]=false;
''')
    needle='    v->v = t;';assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    native_lod_inputs[vtx_idx].uv[0]=s;native_lod_inputs[vtx_idx].uv[1]=t;')
    needle='    native_rsp_profile::Scope profile(native_rsp_profile::batch);';assert text.count(needle)==1
    text=text.replace(needle,'''    unsigned tier=0;
    static const bool native_disable_mesh_lod=getenv("EOG_DISABLE_MESH_LOD")!=nullptr;
    if(native_lod_world_scale>0 && native_lod_strength>0) {
        float nearest=1.e30f;
        for(unsigned i=0;i<entry.count;i++) {
            const auto &t=entry.triangles[i];
            nearest=std::min(nearest,std::min(rsp.loaded_vertices[t.a].w,std::min(rsp.loaded_vertices[t.b].w,rsp.loaded_vertices[t.c].w)));
        }
        tier=native_detail_tier(nearest/native_lod_world_scale,native_lod_strength);
    }
    native_set_texture_lod(tier);
    const NativeMeshLod::Triangle *triangles=entry.triangles;unsigned triangle_count=entry.count;
    bool one_matrix=true,visible=false;
    unsigned matrix_tag=entry.count?native_lod_matrix_tags[entry.triangles[0].a]:0;
    if(tier && !native_disable_mesh_lod && !native_baked_packet)for(unsigned i=0;i<entry.count;i++) {
        const auto &t=entry.triangles[i];
        one_matrix=one_matrix && native_lod_matrix_tags[t.a]==matrix_tag && native_lod_matrix_tags[t.b]==matrix_tag && native_lod_matrix_tags[t.c]==matrix_tag;
        const auto &a=rsp.loaded_vertices[t.a],&b=rsp.loaded_vertices[t.b],&c=rsp.loaded_vertices[t.c];
        bool hidden=(a.clip_rej & b.clip_rej & c.clip_rej)!=0;
        if(native_distance_fog_active){float end=native_distance_fog.end_ndc;hidden=hidden || (end*a.w<=a.z && end*b.w<=b.z && end*c.w<=c.z);}
        visible=visible || !hidden;
    }
    if(tier && !native_baked_packet && one_matrix && visible && !native_disable_mesh_lod && !(rsp.geometry_mode & G_TEXTURE_GEN) && (rdp.other_mode_l & Z_UPD) &&
       (rdp.other_mode_l & ZMODE_DEC)!=ZMODE_DEC && !(rdp.other_mode_l & CVG_X_ALPHA)) {
        if(const auto *lod=native_mesh_lod.get(entry.triangles,entry.count,native_lod_inputs,native_lod_available,native_scenery_kind==1)) {
            triangles=lod->triangles[tier];triangle_count=lod->count[tier];
        }
    }
    if(native_rsp_profile::enabled){native_mesh_tiers[tier]++;native_mesh_input+=entry.count;native_mesh_output+=triangle_count;}
'''+needle)
    # Only the render loop uses the reduced topology; validation and distance
    # selection still see all original corners.
    a=text.index('    auto submit=[&]() {');b=text.index('    submit();return count;',a)
    body=text[a:b].replace('i<entry.count','i<triangle_count').replace('const auto &t=entry.triangles[i];','const auto &t=triangles[i];')
    text=text[:a]+body+text[b:]
    needle='            case NATIVE_SCENERY_COMMAND:';assert text.count(needle)==1
    text=text.replace(needle,'''            case NATIVE_EFFECT_SCOPE_COMMAND: {
                unsigned next=C0(0,3);if(next>4)next=0;
                unsigned wanted=next?next-1:0;
                // Full-resolution cosmetic scopes can share a batch. Flush
                // before changing any active scenery or effective mip level.
                if(native_scenery_active || wanted!=native_texture_tier)gfx_flush();
                native_effect_texture_tier=next;native_set_texture_lod(wanted);
                break;
            }
            case NATIVE_BAKED_LOD_COMMAND:
                native_baked_packet=C0(0,1)?(const NativeBakedLodPacket *)seg_addr(cmd->words.w1):nullptr;
                break;
            case NATIVE_LOD_SCOPE_COMMAND:
                gfx_flush();native_lod_strength=native_setting_lod_strength();native_lod_world_scale=C0(0,1)?((const NativeLodScope *)seg_addr(cmd->words.w1))->world_to_view:0;
                if(!C0(0,1))native_set_texture_lod(0);
                break;
'''+needle)
    needle='                native_scenery_kind=C0(0,2);';assert text.count(needle)==1
    text=text.replace(needle,'''                native_baked_packet=nullptr;
'''+needle)
    needle='    gfx_rapi->start_frame();';text=text.replace(needle,needle+'''
    native_mesh_lod.begin_frame();native_baked_vertices=native_baked_changed=native_baked_removed=0;native_baked_packet=nullptr;native_lod_world_scale=0;native_lod_strength=0;native_texture_tier=native_effect_texture_tier=0;
    memset(native_lod_available,0,sizeof native_lod_available);
    native_mesh_tiers[0]=native_mesh_tiers[1]=native_mesh_tiers[2]=native_mesh_tiers[3]=0;native_mesh_input=native_mesh_output=0;
''')
    needle='            hidden=(a->clip_rej & b->clip_rej & c->clip_rej)!=0;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
            if(native_baked_packet && ((a->x==b->x && a->y==b->y && a->z==b->z && a->w==b->w) ||
               (a->x==c->x && a->y==c->y && a->z==c->z && a->w==c->w) ||
               (b->x==c->x && b->y==c->y && b->z==c->z && b->w==c->w)))hidden=true;
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Baked object LOD: mappedVertices=%u changedVertices=%u removedTriangles=%u",native_baked_vertices,native_baked_changed,native_baked_removed);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Generated LOD: full=%u mid=%u far=%u distant=%u triangles=%u->%u built=%u reused=%u deferred=%u",native_mesh_tiers[0],native_mesh_tiers[1],native_mesh_tiers[2],native_mesh_tiers[3],native_mesh_input,native_mesh_output,native_mesh_lod.builds,native_mesh_lod.hits,native_mesh_lod.deferred);
'''+needle)
    return text
