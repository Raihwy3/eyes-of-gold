"""Run cached opaque index batches through one preparation/packing pass.

Keep original per-triangle clipping for near/fog boundaries, unknown scopes
and debug/reference modes. Material commands, vertex loads, room visibility
and texture lifetime remain in the original interpreter.
"""
def apply(text):
    a=text.index('static void gfx_sp_tri1(uint8_t');b=text.index('static inline void gfx_sp_tri4',a)
    body=text[a:b]
    p=body.index('    auto &use_alpha=native_triangle_setup.use_alpha;')
    q=body.index('    static const bool disable_setup_cache=',p)
    aliases=body[p:q]
    end=body.index('    if (tri_dbg()) {   /* D474',q)
    prepare=body[q:end]
    pack=body.index('    const int n_emit_verts = n_out_tris * 3;',end)
    close=body.rfind('\n}')
    packing=body[pack:close]
    helper='''// The reference triangle path and compiled batches share exact packing.
static void native_prepare_triangles(bool is_rect) {
'''+aliases+prepare+'''}
static void native_pack_triangles(LoadedVertex **emit_v,unsigned n_out_tris,bool is_rect) {
'''+aliases+'''    static const bool disable_setup_cache=getenv("EOG_DISABLE_RSP_PREP_CACHE")!=nullptr;
'''+packing+'''\n}
'''
    body=body[:p]+aliases+'    native_prepare_triangles(is_rect);\n\n'+body[end:pack]+'    native_pack_triangles(emit_v,n_out_tris,is_rect);\n}\n\n'
    text=text[:a]+helper+body+text[b:]
    # Batch code lives after the reference triangle routine, before TRI4.
    needle='static inline void gfx_sp_tri4(Gfx *cmd) {'
    assert text.count(needle)==1
    helper='''#include "native_scenery_batches.h"
static NativeSceneryBatches native_scenery_batches;
static unsigned native_batch_triangles,native_batch_rejected,native_batch_fallback;
static unsigned native_object_fast,native_object_rejected,native_object_fallback;
static bool native_scenery_batch_enabled() {
    static const bool enabled=getenv("EOG_DISABLE_SCENERY_BATCHES")==nullptr &&
        getenv("EOG_DISABLE_RSP_PREP_CACHE")==nullptr;
    return enabled;
}
static unsigned native_run_scenery_batch(Gfx *cmd) {
    if(!native_scenery_active || !native_scenery_batch_enabled() || tri_dbg() ||
       (rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE|G_NO_CLIPPING_EXT)))return 0;
    NativeSceneryBatches::Word words[NativeSceneryBatches::max_commands];unsigned count=0;
    while(count<NativeSceneryBatches::max_commands) {
        const Gfx &g=cmd[count];uint8_t op=g.words.w0>>24;
        if(op!=(uint8_t)G_TRI1 && op!=(uint8_t)G_TRI4)break;
        words[count++]={uint32_t(g.words.w0),uint32_t(g.words.w1)};
    }
    if(!count)return 0;
    const auto &entry=native_scenery_batches.get(words,count,(uint8_t)G_TRI1,(uint8_t)G_TRI4);
    // Validate before touching RSP cache entries. A malformed/extended list
    // falls back as a whole rather than partially consuming its commands.
    for(unsigned i=0;i<entry.count;i++) {
        const auto &t=entry.triangles[i];
        if(t.a>=MAX_VERTICES || t.b>=MAX_VERTICES || t.c>=MAX_VERTICES)return 0;
    }
    native_rsp_profile::Scope profile(native_rsp_profile::batch);
    LoadedVertex *emit[NativeSceneryBatches::max_triangles*3];unsigned pending=0;
    bool prepared=false;
    auto submit=[&]() {
        if(!pending)return;
        if(!prepared){native_prepare_triangles(false);prepared=true;}
        unsigned done=0;
        while(done<pending) {
            // Preserve the reference 256-triangle VBO flush boundaries.
            unsigned span=std::min(pending-done,unsigned(MAX_BUFFERED-buf_vbo_num_tris));
            if(!span){gfx_flush();continue;}
            native_pack_triangles(emit+done*3,span,false);done+=span;
        }
        pending=0;
    };
    for(unsigned i=0;i<entry.count;i++) {
        const auto &t=entry.triangles[i];
        auto *a=&rsp.loaded_vertices[t.a],*b=&rsp.loaded_vertices[t.b],*c=&rsp.loaded_vertices[t.c];
        bool boundary=false,hidden=false,far_hidden=false;
        if(native_distance_fog_active) {
            const float end=native_distance_fog.end_ndc;
            float da=end*a->w-a->z,db=end*b->w-b->z,dc=end*c->w-c->z;
            far_hidden=da<=0 && db<=0 && dc<=0;
            boundary=da<=0 || db<=0 || dc<=0;
        }
        if(!far_hidden) {
            // Behind-camera corners and clip-plane intersections keep the
            // exact original polygon clipping and interpolated attributes.
            boundary=boundary || a->w<=0 || b->w<=0 || c->w<=0 ||
                a->z+a->w<=1e-4f || b->z+b->w<=1e-4f || c->z+c->w<=1e-4f;
            if(boundary) {
                submit();gfx_sp_tri1(t.a,t.b,t.c,false);
                if(native_rsp_profile::enabled){native_batch_fallback++;if(native_scenery_kind==2)native_object_fallback++;}
                continue;
            }
            hidden=(a->clip_rej & b->clip_rej & c->clip_rej)!=0;
            if(!hidden && (rsp.geometry_mode&G_CULL_BOTH)) {
                float cross=native_triangle_facing(a->x,a->y,a->w,b->x,b->y,b->w,c->x,c->y,c->w);
                unsigned mode=rsp.geometry_mode&G_CULL_BOTH;
                hidden=mode==G_CULL_BOTH || (mode==G_CULL_FRONT && cross<=0) || (mode==G_CULL_BACK && cross>=0);
            }
        }
        if(native_rsp_profile::enabled) {
            native_batch_triangles++;
            if(native_scenery_kind==2)native_object_fast++;
            if(native_distance_fog_active){native_distance_seen++;if(far_hidden)native_distance_hidden++;}
        }
        if(far_hidden || hidden){if(native_rsp_profile::enabled){native_batch_rejected++;if(native_scenery_kind==2)native_object_rejected++;}continue;}
        emit[pending*3]=a;emit[pending*3+1]=b;emit[pending*3+2]=c;pending++;
    }
    submit();return count;
}
'''
    text=text.replace(needle,helper+'\n'+needle)
    needle='        uint32_t opcode = cmd->words.w0 >> 24;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
        if(opcode==(uint8_t)G_TRI1 || opcode==(uint8_t)G_TRI4) {
            unsigned taken=native_run_scenery_batch(cmd);
            if(taken){cmd+=taken;continue;}
        }
''')
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'''
    native_scenery_batches.begin_frame();
    native_batch_triangles=native_batch_rejected=native_batch_fallback=0;
    native_object_fast=native_object_rejected=native_object_fallback=0;
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Object batches: fastTriangles=%u rejected=%u fallback=%u",native_object_fast,native_object_rejected,native_object_fallback);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Scenery batches: reused=%u compiled=%u fastTriangles=%u rejected=%u fallback=%u bytes=%u",native_scenery_batches.hits,native_scenery_batches.builds,native_batch_triangles,native_batch_rejected,native_batch_fallback,(unsigned)sizeof native_scenery_batches);
'''+needle)
    return text
