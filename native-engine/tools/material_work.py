"""Avoid material setup work with exact live-state and rectangle-pair proofs."""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    helper='''static unsigned native_material_noops,native_rect_pair_hits;
static bool native_rect_pair_second;
static bool native_rgba_same(const Color &c,uint32_t word) {
    return c.r==((word>>24)&255) && c.g==((word>>16)&255) && c.b==((word>>8)&255) && c.a==(word&255);
}
static bool native_material_noop(Gfx *cmd,uint32_t opcode) {
    static const bool disabled=getenv("EOG_DISABLE_MATERIAL_NOOPS")!=nullptr;
    if(disabled || tri_dbg())return false;
    switch(opcode) {
    case G_RDPPIPESYNC:case G_RDPFULLSYNC:case G_RDPLOADSYNC:case G_RDPTILESYNC:return true;
    case G_SETENVCOLOR:return native_rgba_same(rdp.env_color,cmd->words.w1);
    case G_SETFOGCOLOR:return native_rgba_same(rdp.fog_color,cmd->words.w1);
    case G_SETPRIMCOLOR:return native_rgba_same(rdp.prim_color,cmd->words.w1) &&
        native_rgba_same(rdp.fill_color,cmd->words.w1) && rdp.tex_min_lod==C0(8,8) && rdp.prim_lod_fraction==C0(0,8);
    case G_SETCOMBINE: {
        uint32_t rgb=color_comb(C0(20,4),C1(28,4),C0(15,5),C1(15,3));
        uint32_t alpha=alpha_comb(C0(12,3),C1(12,3),C0(9,3),C1(9,3));
        uint32_t rgb2=color_comb(C0(5,4),C1(24,4),C0(0,5),C1(6,3));
        uint32_t alpha2=alpha_comb(C1(21,3),C1(3,3),C1(18,3),C1(0,3));
        return rdp.combine_mode==(rgb | (alpha<<16) | ((uint64_t)rgb2<<28) | ((uint64_t)alpha2<<44));
    }
    default:return false;
    }
}
'''
    # Color's spelling comes from the upstream RDP state, rather than ABI casts.
    helper=helper.replace('const Color &c','const decltype(rdp.env_color) &c')
    needle='static void gfx_run_dl(Gfx* cmd) {'
    replace(needle,helper+'\n'+needle)
    needle='        uint32_t opcode = cmd->words.w0 >> 24;'
    replace(needle,needle+'''
        if(native_material_noop(cmd,opcode)){if(native_rsp_profile::enabled)native_material_noops++;cmd++;continue;}
''')
    # Share preparation only between the two triangles of one rectangle.
    needle='static bool native_triangle_setup_dirty=true;'
    replace(needle,'static bool native_rect_pair_second;\n'+needle)
    text=text.replace('static bool native_rect_pair_second;\nstatic bool native_rgba_same','static bool native_rgba_same')
    needle='if(native_triangle_setup_dirty || is_rect || disable_setup_cache || tri_dbg()) {'
    replace(needle,'if(native_triangle_setup_dirty || (is_rect && !native_rect_pair_second) || disable_setup_cache || tri_dbg()) {')
    needle='    gfx_sp_tri1(MAX_VERTICES + 0, MAX_VERTICES + 1, MAX_VERTICES + 3, true);\n    gfx_sp_tri1(MAX_VERTICES + 1, MAX_VERTICES + 2, MAX_VERTICES + 3, true);'
    replace(needle,'''    gfx_sp_tri1(MAX_VERTICES + 0, MAX_VERTICES + 1, MAX_VERTICES + 3, true);
    static const bool native_rect_pair_disabled=getenv("EOG_DISABLE_RECT_PAIR_CACHE")!=nullptr;
    if(!native_rect_pair_disabled && !tri_dbg()) {
        native_rect_pair_second=true;native_triangle_setup_dirty=false;
        if(native_rsp_profile::enabled)native_rect_pair_hits++;
    }
    gfx_sp_tri1(MAX_VERTICES + 1, MAX_VERTICES + 2, MAX_VERTICES + 3, true);
    native_rect_pair_second=false;native_triangle_setup_dirty=true;''')
    # Counters must precede the rectangle function as well as the DL dispatcher.
    needle='static unsigned native_material_noops,native_rect_pair_hits;\n';assert text.count(needle)==1
    text=text.replace(needle,'')
    replace('static bool native_rect_pair_second;','static unsigned native_material_noops,native_rect_pair_hits;\nstatic bool native_rect_pair_second;')
    needle='    gfx_rapi->start_frame();'
    replace(needle,needle+'\n    native_material_noops=native_rect_pair_hits=0;native_rect_pair_second=false;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    replace(needle,'    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Material work avoided: stateNoops=%u rectanglePairs=%u",native_material_noops,native_rect_pair_hits);\n'+needle)
    return text
