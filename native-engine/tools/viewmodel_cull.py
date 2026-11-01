"""Record original 1P rejection paths while preserving precise facing."""
def apply(text):
    text='#include "native_viewmodel.h"\nextern "C" void native_pica_set_viewmodel(int) __attribute__((weak));\n'+text
    entry='static void gfx_sp_extra_geometry_mode(uint32_t clear, uint32_t set) {'
    assert text.count(entry)==1
    text=text.replace(entry,entry+'\n    if((((rsp.extra_geometry_mode & ~clear)|set)^rsp.extra_geometry_mode)&NATIVE_VIEWMODEL_SCOPE) {\n        gfx_flush();\n        if(native_pica_set_viewmodel)native_pica_set_viewmodel((((rsp.extra_geometry_mode & ~clear)|set)&NATIVE_VIEWMODEL_SCOPE)!=0);\n    }')
    start=text.index('static void gfx_sp_tri1(uint8_t');end=text.index('static inline void gfx_sp_tri4',start)
    body=text[start:end]
    opening=body.index('{')+1
    body=body[:opening]+'''
    const bool native_vm=(rsp.extra_geometry_mode & NATIVE_VIEWMODEL_SCOPE)!=0;
    const bool native_vm_sample=native_vm && native_rsp_profile::enabled;
    if(native_vm_sample)native_vm_seen++;
'''+body[opening:]
    needle='            // The whole triangle lies outside the visible area'
    assert body.count(needle)==1
    body=body.replace(needle,'''            if(native_vm_sample) {
                native_vm_trivial++;
                unsigned mask=v1->clip_rej & v2->clip_rej & v3->clip_rej;
                if(mask&15)native_vm_xy++;
                if(mask&32) {
                    native_vm_far++;
                    if(native_vm_far==1)sysLogPrintf(LOG_NOTE,"Viewmodel far reject: (%.6g,%.6g,%.6g,%.6g) (%.6g,%.6g,%.6g,%.6g) (%.6g,%.6g,%.6g,%.6g)",
                      (double)v1->x,(double)v1->y,(double)v1->z,(double)v1->w,
                      (double)v2->x,(double)v2->y,(double)v2->z,(double)v2->w,
                      (double)v3->x,(double)v3->y,(double)v3->z,(double)v3->w);
                }
            }
'''+needle)
    needle='        // If inverted culling is requested'
    assert body.count(needle)==1
    body=body.replace(needle,'''        if(native_vm_sample) {
            float precise=native_viewmodel_facing(v1->x,v1->y,v1->w,
                                                 v2->x,v2->y,v2->w,
                                                 v3->x,v3->y,v3->w);
            if(native_vm_sample && ((precise>0)!=(cross>0) || (precise<0)!=(cross<0)))native_vm_corrected++;
        }
'''+needle)
    old='    if ((rsp.geometry_mode & G_CULL_BOTH) != 0) {'
    assert body.count(old)==1
    for comment in ('// entirely behind the near plane','// Why is this even an option?'):
        assert body.count(comment)==1
    body=body.replace('            return; // entirely behind the near plane','            if(native_vm_sample)native_vm_near_drop++;\n            return; // entirely behind the near plane')
    needle='        if (n_in < 3) {'
    assert body.count(needle)==1
    body=body.replace(needle,needle+'\n            if(native_vm_sample)native_vm_near_clip++;')
    for needle in ('                if (cross <= 0) {','                if (cross >= 0) {'):
        assert body.count(needle)==1
        body=body.replace(needle,needle+'\n                    if(native_vm_sample)native_vm_culled++;')
    prefix='static unsigned native_vm_xy,native_vm_far;\nstatic unsigned native_vm_seen,native_vm_corrected,native_vm_trivial,native_vm_culled,native_vm_near_drop,native_vm_near_clip;\n'
    text=text[:start]+prefix+body+text[end:]
    needle='    native_invalidate_packed_vertices();native_packed_hits=native_packed_builds=0;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    native_vm_seen=native_vm_corrected=native_vm_trivial=native_vm_culled=native_vm_near_drop=native_vm_near_clip=0;\n    native_vm_xy=native_vm_far=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    assert text.count(needle)==1
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Viewmodel triangles: seen=%u precisionDisagreements=%u outside=%u culled=%u nearDropped=%u nearClipped=%u xyRejected=%u farRejected=%u",native_vm_seen,native_vm_corrected,native_vm_trivial,native_vm_culled,native_vm_near_drop,native_vm_near_clip,native_vm_xy,native_vm_far);
'''+needle)
    return text
