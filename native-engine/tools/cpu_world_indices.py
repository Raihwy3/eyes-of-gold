"""Identify CPU room draws for diagnostics without changing their array path.

The hint belongs to the complete pending batch, not the last scope seen at
flush. Mixed UI/room batches stay on the reference array path.
"""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    needle='static uint64_t native_vertex_ids[MAX_BUFFERED*3];'
    replace(needle,needle+'''
static bool native_batch_cpu_world;
extern "C" void native_pica_cpu_world_hint(int) __attribute__((weak));
''')
    needle='        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);'
    replace(needle,'''        if(native_pica_cpu_world_hint)native_pica_cpu_world_hint(native_batch_cpu_world);
'''+needle+'''
        if(native_pica_cpu_world_hint)native_pica_cpu_world_hint(0);
''')
    needle='    native_batch_lod_rgb=lod_rgb;native_batch_lod_alpha=lod_alpha;'
    replace(needle,needle+'''
    bool cpu_world=native_scenery_active && native_scenery_kind==1 && !native_gpu_world_active &&
        !(rsp.extra_geometry_mode&(NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE|G_NO_CLIPPING_EXT));
    if(!buf_vbo_num_tris)native_batch_cpu_world=cpu_world;
    else native_batch_cpu_world=native_batch_cpu_world && cpu_world;
''')
    return text
