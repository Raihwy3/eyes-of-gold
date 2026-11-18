"""Carry exact packed-vertex identities to synchronous backend submissions."""
def apply(text):
    needle='static size_t buf_vbo_num_tris;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
static uint64_t native_vertex_ids[MAX_BUFFERED*3];
extern "C" void native_pica_vertex_ids(const float *,const uint64_t *,size_t) __attribute__((weak));
''')
    needle='        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);'
    assert text.count(needle)==1
    text=text.replace(needle,'''        if(native_pica_vertex_ids)native_pica_vertex_ids(buf_vbo,native_vertex_ids,buf_vbo_num_tris*3);
'''+needle+'''
        if(native_pica_vertex_ids)native_pica_vertex_ids(nullptr,nullptr,0);''')
    needle='        if(native_packed && native_packed->generation==native_packed_generation) {'
    assert text.count(needle)==1
    text=text.replace(needle,'''        // A generation owns all packed attributes and positions. Non-RSP
        // corners (near/fog clipping and rectangles) deliberately carry no ID.
        native_vertex_ids[buf_vbo_num_tris*3+i]=native_packed?
            (uint64_t(native_packed_generation)<<8)|unsigned(native_packed-native_packed_vertices+1):0;
'''+needle)
    return text
