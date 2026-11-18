"""Reject model backfaces before material/packing, with reference fallback.

PICA receives only the selected faces and no hardware winding rejection.
The original clipper still owns near/fog crossings; UI/viewmodels are excluded.
"""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    needle='// Return 0 for unknown/mixed matrices, 1 visible/intersecting, 2 outside.'
    helper='''#include "native_model_facing.h"
static unsigned native_face_tested,native_face_rejected,native_face_fallback,native_face_batches;
static NativeModelFacing native_face_matrices[512];
static bool native_face_ready[512];
static const NativeModelFacing &native_face_context(unsigned tag) {
    if(!native_face_ready[tag]){native_face_matrices[tag].set(native_gpu_matrices[tag]);native_face_ready[tag]=true;}
    return native_face_matrices[tag];
}
static bool native_model_face_visible(const NativeModelFacing &facing,const NativeMeshLod::Triangle &t,unsigned mode) {
    if(!mode)return true;
    if(mode==G_CULL_BOTH)return false;
    const auto &a=native_gpu_vertices[t.a],&b=native_gpu_vertices[t.b],&c=native_gpu_vertices[t.c];
    int sign=facing.sign(a.xyz,b.xyz,c.xyz);
    if(!sign) {
        native_gpu_resolve(t.a);native_gpu_resolve(t.b);native_gpu_resolve(t.c);
        const auto &x=rsp.loaded_vertices[t.a],&y=rsp.loaded_vertices[t.b],&z=rsp.loaded_vertices[t.c];
        float cross=native_triangle_facing(x.x,x.y,x.w,y.x,y.y,y.w,z.x,z.y,z.w);
        sign=cross>0?1:cross<0?-1:0;
        if(native_rsp_profile::enabled)native_face_fallback++;
    }
    bool visible=mode==G_CULL_BACK?sign<0:sign>0;
    if(native_rsp_profile::enabled){native_face_tested++;native_face_rejected+=!visible;}
    return visible;
}
'''
    replace(needle,helper+needle)
    # The early batch proof precedes background depth/fog finalization. Do
    # not use original-topology faces to reject a subsequently simplified mesh.
    needle='    if(native_gpu_batch_visibility(entry.triangles,entry.count)==2)return count;'
    replace(needle,'''    unsigned native_batch_visibility=native_gpu_batch_visibility(entry.triangles,entry.count);
    if(native_batch_visibility==2)return count;
    unsigned native_face_mode=rsp.geometry_mode&G_CULL_BOTH;
    if(native_batch_visibility==1 && native_face_mode && !native_baked_packet && !native_lod_strength) {
        const auto &facing=native_face_context(native_gpu_vertices[entry.triangles[0].a].matrix_tag);
        bool visible=false;
        for(unsigned i=0;i<entry.count && !visible;i++)visible=native_model_face_visible(facing,entry.triangles[i],native_face_mode);
        if(!visible){if(native_rsp_profile::enabled)native_face_batches++;return count;}
    }''')
    needle='''        world.cull=(rsp.geometry_mode&G_CULL_BOTH)==G_CULL_FRONT?1:
                   (rsp.geometry_mode&G_CULL_BOTH)==G_CULL_BACK?2:0;'''
    replace(needle,'''        // CPU model-space face selection precedes packing; hardware float24
        // winding must not discard a face that passed the reference predicate.
        world.cull=0;
        const NativeModelFacing *native_facing=native_face_mode?&native_face_context(native_gpu_vertices[triangles[0].a].matrix_tag):nullptr;''')
    needle='            if(hidden)continue;'
    replace(needle,needle+'\n            if(native_face_mode && !native_model_face_visible(*native_facing,t,native_face_mode))continue;')
    needle='    memset(native_gpu_vertices,0,sizeof native_gpu_vertices);'
    replace(needle,needle+'\n    native_face_tested=native_face_rejected=native_face_fallback=native_face_batches=0;memset(native_face_ready,0,sizeof native_face_ready);')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Model face selection: tested=%u rejectedBeforePacking=%u referenceFallbacks=%u batchesSkipped=%u",native_face_tested,native_face_rejected,native_face_fallback,native_face_batches);
'''+needle)
    return text
