"""Carry immutable scenery attribute identities to the real GPU backend."""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    replace('    unsigned generation,count;\n    float values[40];','    unsigned generation,count;\n    uint64_t attribute_id;\n    float values[40];')
    replace('static uint64_t native_vertex_ids[MAX_BUFFERED*3];','''static uint64_t native_vertex_ids[MAX_BUFFERED*3];
static uint64_t native_attribute_ids[MAX_BUFFERED*3];
extern "C" void native_pica_attribute_ids(const float *,const uint64_t *,size_t) __attribute__((weak));''')
    replace('        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);','''        if(native_pica_attribute_ids)native_pica_attribute_ids(buf_vbo,native_attribute_ids,buf_vbo_num_tris*3);
        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        if(native_pica_attribute_ids)native_pica_attribute_ids(nullptr,nullptr,0);''')
    replace('        unsigned native_corner=buf_vbo_num_tris*3+i;','''        unsigned native_corner=buf_vbo_num_tris*3+i;
        native_attribute_ids[native_corner]=native_packed && native_packed->generation==native_packed_generation?native_packed->attribute_id:0;''')
    replace('        const size_t native_pack_start=buf_vbo_len;','''        if(native_packed)native_packed->attribute_id=0;
        const size_t native_pack_start=buf_vbo_len;''')
    replace('native_scenery_cache.read(native_scenery_key,buf_vbo+buf_vbo_len,count)','native_scenery_cache.read(native_scenery_key,buf_vbo+buf_vbo_len,count,&native_packed->attribute_id)')
    replace('                buf_vbo_len+=count;\n                if(buf_vbo_len-native_pack_start<=40)', '                native_attribute_ids[native_corner]=native_packed->attribute_id;\n                buf_vbo_len+=count;\n                if(buf_vbo_len-native_pack_start<=40)')
    replace('        if(native_scenery_store)native_scenery_cache.write(native_scenery_key,buf_vbo+native_pack_start+4,buf_vbo_len-native_pack_start-4);','''        if(native_scenery_store) {
            native_packed->attribute_id=native_scenery_cache.write(native_scenery_key,buf_vbo+native_pack_start+4,buf_vbo_len-native_pack_start-4);
            native_attribute_ids[native_corner]=native_packed->attribute_id;
        }''')
    return text
