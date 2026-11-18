"""Persistent camera-independent attributes for explicitly scoped opaque BG.

Current homogeneous positions and fog factors are never cached. Materials,
colours and UV state participate in exact keys. Depth-based colour LOD is
recalculated on hits; clip-created corners use the reference path. Memory is fixed, no room/GPU
allocation or lifetime is retained.
"""
def apply(text):
    needle='// Material generations own cached vertex bytes.'
    assert text.count(needle)==1
    text=text.replace(needle,'''#include "native_scenery_cache.h"
#include "native_scenery_scope.h"
struct NativeSceneryContext {
    decltype(native_triangle_setup) setup;
    RGBA prim,env,grey,fog;
    uint32_t mode,colour;
    uint8_t mapping[2][7],lod;
};
static NativeSceneryCache<NativeSceneryContext> native_scenery_cache;
static_assert(sizeof(native_scenery_cache)<=256*1024,"Scenery cache memory budget");
static bool native_scenery_active;
static unsigned native_scenery_kind;
static uint32_t native_scenery_context;
static bool native_scenery_depth_lod;
static bool native_object_batch_enabled() {
    static const bool enabled=getenv("EOG_DISABLE_OBJECT_BATCHES")==nullptr;
    return enabled;
}
static bool native_scenery_enabled() {
    static const bool enabled=getenv("EOG_DISABLE_SCENERY_CACHE")==nullptr;
    return enabled;
}
'''+needle)
    needle='        native_triangle_setup_dirty=is_rect;'
    assert text.count(needle)==1
    text=text.replace(needle,'''        native_scenery_context=0;native_scenery_depth_lod=false;
        if(native_scenery_active && native_scenery_enabled() && !is_rect &&
           !(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE))) {

            if(rdp.other_mode_h & G_TL_LOD)for(unsigned k=0;k<2;k++)for(unsigned j=0;j<num_inputs;j++)
                if(comb->shader_input_mapping[k][j]==G_CCMUX_LOD_FRACTION)native_scenery_depth_lod=true;
            NativeSceneryContext key;memset(&key,0,sizeof key);
            key.setup=native_triangle_setup;
            key.setup.comb=nullptr; // Input mapping, not allocator address, owns attributes.
            key.prim=rdp.prim_color;key.env=rdp.env_color;key.grey=rdp.grayscale_color;key.fog=rdp.fog_color;
            key.mode=rdp.other_mode_h;key.lod=rdp.prim_lod_fraction;
            key.colour=native_distance_fog_active?native_distance_fog.colour:0;
            memcpy(key.mapping,comb->shader_input_mapping,sizeof key.mapping);
            native_scenery_context=native_scenery_cache.context(key);
        }
'''+needle)
    needle='        for (int t = 0; t < 2; t++) {'
    a=text.index('    const int n_emit_verts');pos=text.index(needle,a)
    text=text[:pos]+'''        NativeSceneryCache<NativeSceneryContext>::VertexKey native_scenery_key{};
        bool native_scenery_store=false;
        if(native_scenery_active && native_scenery_context && native_packed && !is_rect) {
            native_scenery_key=NativeSceneryCache<NativeSceneryContext>::key(native_scenery_context,vi->u,vi->v,&vi->color);
            unsigned count;
            if(native_scenery_cache.read(native_scenery_key,buf_vbo+buf_vbo_len,count)) {
                if(use_fog) {
                    unsigned off=0;
                    for(unsigned t=0;t<2;t++)if(used_textures[t])off+=2+((tm>>(2*t))&1)+((tm>>(2*t+1))&1);
                    assert(off+3<count);buf_vbo[buf_vbo_len+off+3]=vi->fog_n/255.0f;
                }
                if(native_scenery_depth_lod) {
                    unsigned off=(use_fog?4:0)+(use_grayscale?4:0);
                    for(unsigned t=0;t<2;t++)if(used_textures[t])off+=2+((tm>>(2*t))&1)+((tm>>(2*t+1))&1);
                    const float distance_frac=std::max(0.f,std::min(vi->w/1024.f,1.f));
                    const uint8_t lod=(0.7f+distance_frac*0.3f)*255.f;
                    for(unsigned j=0;j<num_inputs;j++) {
                        unsigned at=off+j*(use_alpha?4:3);assert(at+2<count);
                        if(comb->shader_input_mapping[0][j]==G_CCMUX_LOD_FRACTION)
                            for(unsigned k=0;k<3;k++)buf_vbo[buf_vbo_len+at+k]=native_unorm8(lod);
                        if(use_alpha && comb->shader_input_mapping[1][j]==G_CCMUX_LOD_FRACTION) {
                            assert(at+3<count);buf_vbo[buf_vbo_len+at+3]=native_unorm8(lod);
                        }
                    }
                }
                buf_vbo_len+=count;
                if(buf_vbo_len-native_pack_start<=40) {
                    native_packed->count=buf_vbo_len-native_pack_start;
                    memcpy(native_packed->values,buf_vbo+native_pack_start,native_packed->count*sizeof(float));
                    native_packed->generation=native_packed_generation;
                }
                continue;
            }
            native_scenery_store=true;
        }
'''+text[pos:]
    needle='        if(native_packed && buf_vbo_len-native_pack_start<=40) {'
    assert text.count(needle)==1
    text=text.replace(needle,'''        if(native_scenery_store)native_scenery_cache.write(native_scenery_key,buf_vbo+native_pack_start+4,buf_vbo_len-native_pack_start-4);
'''+needle)
    needle='            case NATIVE_DISTANCE_FOG_COMMAND:'
    assert text.count(needle)==1
    text=text.replace(needle,'''            case NATIVE_SCENERY_COMMAND:
                native_scenery_kind=C0(0,2);
                native_scenery_active=native_scenery_kind==1 || (native_scenery_kind==2 && native_object_batch_enabled());native_scenery_context=0;
                native_triangle_setup_dirty=true;native_invalidate_packed_vertices();
                break;
'''+needle)
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'''
    native_scenery_active=false;native_scenery_kind=0;native_scenery_context=0;
    native_scenery_cache.begin_frame();
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Scenery attributes: reused=%u persisted=%u built=%u bytes=%u",native_scenery_cache.hits,native_scenery_cache.cross_frame_hits,native_scenery_cache.misses,(unsigned)sizeof native_scenery_cache);
'''+needle)
    return text
