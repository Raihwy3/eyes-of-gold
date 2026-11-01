"""Reject before clipping fallback and avoid general floorf for byte colours."""
import re

def apply(text):
    text='#include "native_clip_colour.h"\n'+text
    needle='static void gfx_sp_tri1(uint8_t'
    at=text.index(needle)
    text=text[:at]+'''static unsigned native_early_fallback_rejects,native_clip_fast_colours;
static uint8_t native_clip_round(float value) {
    static const bool reference=getenv("EOG_DISABLE_CLIP_COLOUR_FASTPATH")!=nullptr;
    if(reference)return (uint8_t)clampf(floorf(value+.5f),0.f,255.f);
    if(native_rsp_profile::enabled)native_clip_fast_colours++;
    return native_clip_colour(value);
}
'''+text[at:]
    # Six original near/fog clipping sites; do not touch texture preparation.
    a=text.index(needle);b=text.index('#include "native_scenery_batches.h"',a)
    body=text[a:b]
    pattern=r'\(uint8_t\)clampf\(floorf\((.*?)\s*\+\s*(?:0\.5f|\.5f)\),\s*0\.f,\s*255\.f\)'
    body,count=re.subn(pattern,lambda m:'native_clip_round('+m[1]+')',body)
    assert count==6,count
    text=text[:a]+body+text[b:]
    a=text.index('    native_set_gpu_world(nullptr);\n    for(unsigned i=0;i<triangle_count;i++) {')
    b=text.index('    submit();return count;',a)
    body=text[a:b]
    needle='            if(boundary) {'
    assert body.count(needle)==1
    body=body.replace(needle,'''            static const bool reference_cull=getenv("EOG_DISABLE_EARLY_WORLD_CULL")!=nullptr;
            if(boundary && !reference_cull) {
                // Match the reference rejection order and negative-W guard.
                // Rejecting here avoids flushing pending triangles and then
                // entering gfx_sp_tri1 only to reject the same triangle.
                bool behind=a->w<0 || b->w<0 || c->w<0;
                bool reject=!behind && (a->clip_rej & b->clip_rej & c->clip_rej)!=0;
                if(!reject && (rsp.geometry_mode&G_CULL_BOTH)) {
                    float cross=native_triangle_facing(a->x,a->y,a->w,b->x,b->y,b->w,c->x,c->y,c->w);
                    unsigned mode=rsp.geometry_mode&G_CULL_BOTH;
                    reject=mode==G_CULL_BOTH || (mode==G_CULL_FRONT && cross<=0) || (mode==G_CULL_BACK && cross>=0);
                }
                if(reject) {
                    if(native_rsp_profile::enabled) {
                        native_early_fallback_rejects++;native_batch_triangles++;native_batch_rejected++;
                        if(native_scenery_kind==2){native_object_fast++;native_object_rejected++;}
                        if(native_distance_fog_active)native_distance_seen++;
                    }
                    continue;
                }
            }
'''+needle)
    text=text[:a]+body+text[b:]
    needle='static void native_pack_triangles(LoadedVertex **emit_v,unsigned n_out_tris,bool is_rect) {'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    native_rsp_profile::Scope packing_profile(native_rsp_profile::packing);')
    needle='    gfx_rapi->start_frame();'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    native_early_fallback_rejects=native_clip_fast_colours=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"CPU clipping: avoidedFallback=%u fastColourRounds=%u",native_early_fallback_rejects,native_clip_fast_colours);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"RSP exclusive: matrices=%.2fms vertices=%.2fms triangles=%.2fms batches=%.2fms packing=%.2fms backend=%.2fms (probe; no nested double counting)",native_rsp_profile::exclusive_ms(native_rsp_profile::matrix),native_rsp_profile::exclusive_ms(native_rsp_profile::vertex),native_rsp_profile::exclusive_ms(native_rsp_profile::triangle),native_rsp_profile::exclusive_ms(native_rsp_profile::batch),native_rsp_profile::exclusive_ms(native_rsp_profile::packing),native_rsp_profile::exclusive_ms(native_rsp_profile::backend));
'''+needle)
    return text
