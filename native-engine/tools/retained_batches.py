"""Keep model-space batch templates and GPU meshes across frames.

Exact material/position/UV/shade/topology proofs own a monotonic token. View
matrices, fog factors and camera LOD are deliberately dynamic. Unknown cases
keep the reference packing path.
"""
def apply(text):
    needle='static unsigned native_run_scenery_batch(Gfx *cmd) {'
    helper=r'''
extern "C" void native_pica_retained_hint(uint64_t) __attribute__((weak));
#include "native_retained_index.h"
struct NativeRetainedBatch {
    struct Input {float xyz[3],u,v;};
    NativeSceneryContext material;
    Input inputs[32];uint8_t indices[192];
    float data[32][40];uint64_t attributes[32];
    uint64_t token=0,last_used=0;unsigned vertices=0,corners=0,stride=0,hash=0;
};
static NativeRetainedBatch native_retained_batches[256];
static NativeRetainedIndex<256,64> native_retained_index;
static uint64_t native_retained_serial,native_retained_clock;
static unsigned native_retained_hits,native_retained_builds,native_retained_corners;
static unsigned native_retained_lookups,native_retained_misses,native_retained_evictions;
static bool native_replay_gpu(LoadedVertex **emit,unsigned triangles) {
    static const bool disabled=getenv("EOG_DISABLE_RETAINED_MESHES")!=nullptr;
    if(disabled || !native_pica_retained_hint || !native_pica_retained_supported || !native_pica_retained_supported() || !native_gpu_world_active || !native_reference_enabled() ||
       !triangles || triangles>64 || !native_scenery_context || native_triangle_setup.use_grayscale)return false;
    NativeRetainedBatch::Input inputs[32];uint8_t indices[192],slots[32],remap[MAX_VERTICES];
    memset(remap,255,sizeof remap);unsigned vertices=0,corners=triangles*3;
    for(unsigned i=0;i<corners;i++) {
        uintptr_t offset=(uintptr_t)emit[i]-(uintptr_t)rsp.loaded_vertices;
        if(offset>=sizeof rsp.loaded_vertices || offset%sizeof(LoadedVertex))return false;
        unsigned slot=offset/sizeof(LoadedVertex),j=remap[slot];
        if(j==255) {
            if(vertices==32)return false;j=vertices++;remap[slot]=j;slots[j]=slot;
            const auto &v=*emit[i];const auto &g=native_gpu_vertices[slot];
            if(!g.valid)return false;
            memcpy(inputs[j].xyz,g.xyz,sizeof g.xyz);inputs[j].u=v.u;inputs[j].v=v.v;
        }
        indices[i]=j;
    }
    const auto &material=*native_scenery_cache.current_context;
    unsigned hash=native_scenery_cache.current_hash;
    auto mix=[&](const void *p,unsigned bytes) {
        const auto *b=(const uint8_t *)p;for(unsigned i=0;i<bytes;i++)hash=(hash^b[i])*16777619u;
    };
    mix(inputs,vertices*sizeof inputs[0]);mix(indices,corners);hash^=hash>>16;
    if(native_rsp_profile::enabled)native_retained_lookups++;
    int found_index=native_retained_index.find(hash,[&](unsigned i) {
        const auto &candidate=native_retained_batches[i];
        return candidate.token && candidate.hash==hash && candidate.vertices==vertices && candidate.corners==corners &&
           !memcmp(&candidate.material,&material,sizeof material) && !memcmp(candidate.inputs,inputs,vertices*sizeof inputs[0]) &&
           !memcmp(candidate.indices,indices,corners);
    });
    bool found=found_index>=0;
    if(!found && native_rsp_profile::enabled)native_retained_misses++;
    if(!found && native_retained_builds>=16)return false;
    // A dedicated synchronous producer draw gives the backend an unambiguous
    // token/span and consumes all template references before they can change.
    gfx_flush();
    unsigned entry_index=found?unsigned(found_index):native_retained_index.victim();
    auto &entry=native_retained_batches[entry_index];
    if(!found) {
        native_pack_triangles(emit,triangles,false);
        unsigned stride=buf_vbo_len/corners;
        if(stride>40 || stride*corners!=buf_vbo_len){gfx_flush();return true;}
        entry.material=material;entry.vertices=vertices;entry.corners=corners;entry.hash=hash;entry.stride=stride;
        if(entry.token && native_rsp_profile::enabled)native_retained_evictions++;
        memcpy(entry.inputs,inputs,vertices*sizeof inputs[0]);memcpy(entry.indices,indices,corners);
        bool copied[32]={};
        for(unsigned i=0;i<corners;i++) {
            unsigned j=indices[i];if(copied[j])continue;copied[j]=true;
            memcpy(entry.data[j],native_vertex_refs[i],stride*sizeof(float));entry.attributes[j]=native_attribute_ids[i];
        }
        // Tokens are never recycled across stages or cache replacement.
        entry.token=++native_retained_serial;if(!entry.token)abort();native_retained_builds++;
        native_retained_index.install(entry_index,hash);
    }else {
        const auto &setup=native_triangle_setup;
        unsigned texture_floats=0;
        for(unsigned t=0;t<2;t++)if(setup.used_textures[t])texture_floats+=2+((setup.tm>>(2*t))&1)+((setup.tm>>(2*t+1))&1);
        unsigned fog_offset=4+texture_floats,channels=setup.use_alpha?4:3;
        unsigned input_offset=fog_offset+(setup.use_fog?4:0);
        for(unsigned i=0;i<vertices;i++) {
            const auto &v=rsp.loaded_vertices[slots[i]];float *p=entry.data[i];
            if(setup.use_fog)p[fog_offset+3]=v.fog_n/255.f;
            // Geometry/UVs survive pose and damage-colour changes. Refresh
            // only shade inputs, once per unique vertex rather than corner.
            for(unsigned j=0;j<setup.num_inputs;j++) {
                unsigned at=input_offset+j*channels;
                unsigned rgb=setup.comb->shader_input_mapping[0][j];
                if(rgb==G_CCMUX_SHADE){p[at]=native_unorm8(v.color.r);p[at+1]=native_unorm8(v.color.g);p[at+2]=native_unorm8(v.color.b);}
                if(rgb==G_CCMUX_SHADE_ALPHA)for(unsigned k=0;k<3;k++)p[at+k]=native_unorm8(v.color.a);
                if(setup.use_alpha && setup.comb->shader_input_mapping[1][j]==G_CCMUX_SHADE)p[at+3]=native_unorm8(v.color.a);
            }
            if(rdp.other_mode_h&G_TL_LOD) {
                float fraction=std::max(0.f,std::min(v.w/1024.f,1.f));
                float value=native_unorm8(uint8_t((.7f+fraction*.3f)*255.f));
                for(unsigned j=0;j<setup.num_inputs;j++) {
                    if(setup.comb->shader_input_mapping[0][j]==G_CCMUX_LOD_FRACTION)
                        for(unsigned k=0;k<3;k++)p[input_offset+j*channels+k]=value;
                    if(setup.use_alpha && setup.comb->shader_input_mapping[1][j]==G_CCMUX_LOD_FRACTION)p[input_offset+j*channels+3]=value;
                }
            }
        }
        float fog[3]={};native_batch_metadata(fog);
        for(unsigned i=0;i<corners;i++) {
            unsigned j=indices[i];native_vertex_refs[i]=entry.data[j];
            native_vertex_ids[i]=(uint64_t(native_packed_generation)<<8)|(slots[j]+1);
            native_attribute_ids[i]=0; // Shade is dynamic; old static-attribute IDs do not apply.
        }
        buf_vbo_len=corners*entry.stride;buf_vbo_num_tris=triangles;
        native_retained_hits++;native_retained_corners+=corners;
    }
    entry.last_used=++native_retained_clock;
    native_retained_index.touch(entry_index);
    native_pica_retained_hint(entry.token);gfx_flush();native_pica_retained_hint(0);
    return true;
}
'''
    assert text.count(needle)==1
    text=text.replace(needle,helper+'\n'+needle)
    needle='            native_pack_triangles(emit+done*3,span,false);done+=span;'
    # The earlier prepared CPU helper has a similarly named loop. Only change
    # the common submit lambda after the new helper.
    at=text.index('    auto submit=[&]() {',text.index('static unsigned native_run_scenery_batch'))
    end=text.index('    bool gpu=',at)
    body=text[at:end];assert body.count(needle)==1
    body=body.replace(needle,'''            if(!native_gpu_world_active || !native_replay_gpu(emit+done*3,span))native_pack_triangles(emit+done*3,span,false);
            done+=span;''')
    text=text[:at]+body+text[end:]
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'\n    native_retained_hits=native_retained_builds=native_retained_corners=0;native_retained_lookups=native_retained_misses=native_retained_evictions=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Retained mesh producer: hits=%u builds=%u reusedCorners=%u bytes=%u",native_retained_hits,native_retained_builds,native_retained_corners,(unsigned)sizeof native_retained_batches);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Retained mesh index: lookups=%u misses=%u capacityEvictions=%u bytes=%u",native_retained_lookups,native_retained_misses,native_retained_evictions,(unsigned)sizeof native_retained_index);
'''+needle)
    return text
