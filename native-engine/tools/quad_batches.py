"""Pack proven rectangle pairs without general model-triangle processing."""
def apply(text):
    a=text.index('    gfx_sp_tri1(MAX_VERTICES + 0, MAX_VERTICES + 1, MAX_VERTICES + 3, true);')
    b=text.index('    native_rect_pair_second=false;native_triangle_setup_dirty=true;',a)+len('    native_rect_pair_second=false;native_triangle_setup_dirty=true;')
    old=text[a:b]
    new='''    static const bool native_quad_disabled=getenv("EOG_DISABLE_QUAD_BATCHES")!=nullptr;
    bool native_quad=!native_quad_disabled && !tri_dbg() && !(rsp.geometry_mode&G_CULL_BOTH) &&
        MAX_BUFFERED-buf_vbo_num_tris>=2;
    LoadedVertex *native_corners[]={ul,ll,lr,ur};
    for(auto *v:native_corners)native_quad=native_quad && !v->clip_rej && v->w>0 && v->z==-1.f &&
        __builtin_isfinite(v->x) && __builtin_isfinite(v->y) && __builtin_isfinite(v->w);
    if(native_quad) {
        // These exact helpers create clip-space corners, no near/fog clipping,
        // and geometry mode zero. XY clipping remains with the GPU/scissor.
        // Preserve the retail two-triangle order and the 256-triangle limit.
        native_set_gpu_world(nullptr);
        native_prepare_triangles(true);
        LoadedVertex *emit[]={ul,ll,ur,ll,lr,ur};
        native_pack_triangles(emit,2,true);
        native_triangle_setup_dirty=true;
        if(native_rsp_profile::enabled)native_quad_batches++;
    }else {
'''+old+'''
    }
'''
    text=text[:a]+new+text[b:]
    needle='static unsigned native_material_noops,native_rect_pair_hits;';assert text.count(needle)==1
    text=text.replace(needle,needle+'\nstatic unsigned native_quad_batches;')
    needle='    native_material_noops=native_rect_pair_hits=0;native_rect_pair_second=false;';assert text.count(needle)==1
    text=text.replace(needle,needle+'native_quad_batches=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;';assert text.count(needle)==1
    text=text.replace(needle,'    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Direct UI quads: batches=%u",native_quad_batches);\n'+needle)
    return text
