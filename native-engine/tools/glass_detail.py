"""Cheap pure-glass material without altering visibility, hits or destruction."""
def apply(text):
    text='#include "native_glass.h"\nstatic int native_glass_detail;\nstatic unsigned native_glass_lod,native_glass_triangles,native_glass_gpu_triangles,native_glass_scopes;\nstatic unsigned long long native_glass_ticks,native_glass_start;\n'+text
    entry='static void gfx_sp_extra_geometry_mode(uint32_t clear, uint32_t set) {'
    assert text.count(entry)==1
    text=text.replace(entry,entry+'''
    if(((((rsp.extra_geometry_mode & ~clear)|set)^rsp.extra_geometry_mode)&NATIVE_GLASS_SCOPE)!=0) {
        gfx_flush();
        if(native_rsp_profile::enabled && (rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE))native_glass_ticks+=native_rsp_profile::now()-native_glass_start;
        native_glass_detail=native_setting_glass();native_glass_lod=0;
        if(native_rsp_profile::enabled && (((rsp.extra_geometry_mode & ~clear)|set)&NATIVE_GLASS_SCOPE)){native_glass_start=native_rsp_profile::now();native_glass_scopes++;}
        native_invalidate_packed_vertices();
    }
''')
    # These calculations have no consumer in the flat-tint material. Preserve
    # clip coordinates and original vertex bytes for every other draw.
    a=text.index('static void gfx_sp_vertex(');b=text.index('static void gfx_sp_tri1(',a)
    body=text[a:b]
    old='if (rsp.geometry_mode & G_LIGHTING) {'
    assert body.count(old)==2
    body=body.replace(old,'if ((rsp.geometry_mode & G_LIGHTING) && !((rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE) && native_glass_detail)) {')
    text=text[:a]+body+text[b:]
    a=text.index('static void gfx_sp_tri1(')
    guard='''// Restore all authored RDP state even when clipping returns early.
struct NativeGlassMaterial {
    bool enabled;uint64_t combine;uint32_t low,high;RGBA prim;
    explicit NativeGlassMaterial(bool on):enabled(on) {
        if(!on)return;
        combine=rdp.combine_mode;low=rdp.other_mode_l;high=rdp.other_mode_h;prim=rdp.prim_color;
        // (0 - 0) * 0 + PRIMITIVE in both colour and alpha channels.
        const uint64_t rgb=15u|(15u<<4)|(31u<<8)|(3u<<13);
        const uint64_t alpha=7u|(7u<<3)|(7u<<6)|(3u<<9);
        rdp.combine_mode=rgb|(alpha<<16)|(rgb<<28)|(alpha<<44);
        unsigned opacity=native_glass_alpha(prim.a);
        rdp.prim_color={12,12,12,(uint8_t)opacity};
        rdp.other_mode_h&=~(3u<<G_MDSFT_CYCLETYPE);
        rdp.other_mode_l=opacity==255?(G_RM_AA_ZB_OPA_SURF|G_RM_AA_ZB_OPA_SURF2):(G_RM_AA_ZB_XLU_SURF|G_RM_AA_ZB_XLU_SURF2);
    }
    ~NativeGlassMaterial(){if(enabled){rdp.combine_mode=combine;rdp.other_mode_l=low;rdp.other_mode_h=high;rdp.prim_color=prim;}}
};
'''
    text=text[:a]+guard+text[a:]
    entry='static void gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {'
    assert text.count(entry)==1
    text=text.replace(entry,entry+'''
    const bool glass=(rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE)!=0 && !is_rect;
    if(glass && native_rsp_profile::enabled)native_glass_triangles++;
    NativeGlassMaterial glass_material(glass && native_glass_detail==1);
''')
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'\n    native_glass_triangles=native_glass_gpu_triangles=native_glass_scopes=0;native_glass_ticks=native_glass_start=0;native_glass_lod=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    assert text.count(needle)==1
    text=text.replace(needle,'''#ifdef __3DS__
    const double native_glass_ms=double(native_glass_ticks)*1000.0/native_rsp_profile::tick_frequency;
#else
    const double native_glass_ms=double(native_glass_ticks)/1.e6;
#endif
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Glass: setting=%d triangles=%u gpuTriangles=%u scopes=%u cpuWall=%.2fms",native_setting_glass(),native_glass_triangles,native_glass_gpu_triangles,native_glass_scopes,native_glass_ms);
'''+needle)
    return text
