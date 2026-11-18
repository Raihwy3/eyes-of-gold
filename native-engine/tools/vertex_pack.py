"""Reuse exact packed attributes of unchanged RSP vertex-cache entries."""
def apply(text):
    signature='static void gfx_sp_tri1(uint8_t'
    start=text.index(signature);end=text.index('static inline void gfx_sp_tri4',start)
    body=text[start:end]
    prefix='''// Material generations own cached vertex bytes.
static struct NativePackedVertex {
    unsigned generation,count;
    float values[40];
} native_packed_vertices[MAX_VERTICES];
static unsigned native_packed_generation=1,native_packed_hits,native_packed_builds;
static inline void native_invalidate_packed_vertices(void) {
    if(++native_packed_generation==0) {
        memset(native_packed_vertices,0,sizeof native_packed_vertices);
        native_packed_generation=1;
    }
}
'''
    needle='    if(native_triangle_setup_dirty || is_rect || disable_setup_cache || tri_dbg()) {'
    assert body.count(needle)==1
    body=body.replace(needle,needle+'\n        native_invalidate_packed_vertices();')
    needle='        struct LoadedVertex* vi = emit_v[i];'
    assert body.count(needle)==1
    body=body.replace(needle,needle+'''
        NativePackedVertex *native_packed=nullptr;
        if(!is_rect && !disable_setup_cache && !tri_dbg()) {
            const uintptr_t offset=(uintptr_t)vi-(uintptr_t)&rsp.loaded_vertices[0];
            if(offset<MAX_VERTICES*sizeof(LoadedVertex) && offset%sizeof(LoadedVertex)==0)
                native_packed=&native_packed_vertices[offset/sizeof(LoadedVertex)];
        }
        if(native_packed && native_packed->generation==native_packed_generation) {
            memcpy(buf_vbo+buf_vbo_len,native_packed->values,native_packed->count*sizeof(float));
            buf_vbo_len+=native_packed->count;
            if(native_rsp_profile::enabled)native_packed_hits++;
            continue;
        }
        const size_t native_pack_start=buf_vbo_len;
''')
    needle='    buf_vbo_num_tris += n_out_tris;'
    pos=body.index(needle);close=body.rfind('    }',0,pos)
    body=body[:close]+'''        if(native_packed && buf_vbo_len-native_pack_start<=40) {
            native_packed->count=buf_vbo_len-native_pack_start;
            memcpy(native_packed->values,buf_vbo+native_pack_start,native_packed->count*sizeof(float));
            native_packed->generation=native_packed_generation;
            if(native_rsp_profile::enabled)native_packed_builds++;
        }
'''+body[close:]
    text=text[:start]+prefix+body+text[end:]
    needle='        uint32_t opcode = cmd->words.w0 >> 24;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
        switch(opcode) {
          case (uint8_t)G_TRI1: case (uint8_t)G_TRI4:
          case (uint8_t)G_DL: case (uint8_t)G_ENDDL: break;
          default: native_invalidate_packed_vertices();break;
        }
''')
    needle='    native_triangle_setup_hits=native_triangle_setup_builds=0;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
    native_invalidate_packed_vertices();native_packed_hits=native_packed_builds=0;
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    assert text.count(needle)==1
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"RSP vertex packing: reused=%u packed=%u",native_packed_hits,native_packed_builds);
'''+needle)
    return text
