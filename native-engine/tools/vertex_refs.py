"""Keep shared packed corners as immutable references inside a pending batch."""
def apply(text):
    needle='static uint64_t native_vertex_ids[MAX_BUFFERED*3];'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
static const float *native_vertex_refs[MAX_BUFFERED*3];
extern "C" void native_pica_vertex_refs(const float *,const float *const *,size_t) __attribute__((weak));
static struct NativeReferenceSlot {uint64_t id;const float *data;unsigned epoch;} native_reference_slots[128];
static unsigned native_reference_epoch=1,native_reference_corners,native_reference_floats;
static bool native_reference_enabled() {
    static const bool enabled=getenv("EOG_DISABLE_VERTEX_REFS")==nullptr;
    return enabled && native_pica_vertex_refs;
}
''')
    needle='        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);'
    text=text.replace(needle,'''        if(native_pica_vertex_refs)native_pica_vertex_refs(buf_vbo,native_reference_enabled()?native_vertex_refs:nullptr,buf_vbo_num_tris*3);
'''+needle+'''
        if(native_pica_vertex_refs)native_pica_vertex_refs(nullptr,nullptr,0);
        if(++native_reference_epoch==0){memset(native_reference_slots,0,sizeof native_reference_slots);native_reference_epoch=1;}
''')
    needle='        if(native_packed && native_packed->generation==native_packed_generation) {'
    assert text.count(needle)==1
    text=text.replace(needle,'''        unsigned native_corner=buf_vbo_num_tris*3+i;
        native_vertex_refs[native_corner]=buf_vbo+buf_vbo_len;
        if(native_packed && native_reference_enabled()) {
            auto &slot=native_reference_slots[native_packed-native_packed_vertices];
            uint64_t id=native_vertex_ids[native_corner];
            if(slot.epoch==native_reference_epoch && slot.id==id && native_packed->generation==native_packed_generation) {
                native_vertex_refs[native_corner]=slot.data;
                buf_vbo_len+=native_packed->count;
                if(native_rsp_profile::enabled){native_reference_corners++;native_reference_floats+=native_packed->count;native_packed_hits++;}
                continue;
            }
            slot={id,buf_vbo+buf_vbo_len,native_reference_epoch};
        }
'''+needle)
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'\n    native_reference_corners=native_reference_floats=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Vertex packing refs: corners=%u floatCopiesAvoided=%u",native_reference_corners,native_reference_floats);
'''+needle)
    return text
