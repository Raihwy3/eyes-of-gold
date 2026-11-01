"""Replay immutable packed batch templates before per-corner packing."""
def apply(text):
    # Share the original batch-hint bookkeeping with replay. This preserves
    # mixed pending material/fog proofs exactly instead of replaying stale
    # hints captured alongside an earlier pending batch.
    start=text.index('    unsigned lod_rgb=0,lod_alpha=0;',text.index('static void native_pack_triangles('))
    end=text.index('    for (int i = 0; i < n_emit_verts; i++) {',start)
    metadata=text[start:end]
    metadata=metadata.replace('    float native_fog_colour[3]={};\n','').replace('sizeof native_fog_colour','3*sizeof(float)')
    helper_meta='''static void native_batch_metadata(float *native_fog_colour) {
    auto &use_alpha=native_triangle_setup.use_alpha;
    auto &use_fog=native_triangle_setup.use_fog;
    auto &num_inputs=native_triangle_setup.num_inputs;
    auto &comb=native_triangle_setup.comb;
'''+metadata+'}\n'
    text=text[:start]+'    float native_fog_colour[3]={};native_batch_metadata(native_fog_colour);\n'+text[end:]
    at=text.index('static void native_pack_triangles(')
    text=text[:at]+helper_meta+text[at:]
    needle='static unsigned native_run_scenery_batch(Gfx *cmd) {'
    assert text.count(needle)==1
    helper=r'''
#include "native_retained_index.h"
struct NativePreparedBatch {
    unsigned context=0,triangles=0,vertices=0,stride=0,live_epoch=0;
    NativeSceneryContext material;uint64_t last_used=0;bool ready[32]{};
    uint8_t indices[192],slots[32];
    struct Attribute {float u,v;RGBA colour;} attributes[32];
    float data[32][40];uint64_t attribute_ids[32];
    unsigned lod_rgb=0,lod_alpha=0;
};
static NativePreparedBatch native_prepared_batches[128];
static NativeRetainedIndex<128,32> native_prepared_index;
static unsigned native_prepared_hits,native_prepared_misses,native_prepared_corners,native_prepared_builds;
static unsigned native_prepared_replaced;
static uint64_t native_prepared_clock;
static unsigned native_prepared_key_miss,native_prepared_partial,native_prepared_inflight,native_prepared_limit,native_prepared_capacity,native_prepared_boundary;
static bool native_replay_batch(const NativeMeshLod::Triangle *triangles,unsigned triangle_count) {
    static const bool disabled=getenv("EOG_DISABLE_PREPARED_BATCHES")!=nullptr;
    if(disabled || !native_reference_enabled() || !native_scenery_enabled() || native_gpu_world_available() ||
       !triangle_count || triangle_count>64 || buf_vbo_num_tris>MAX_BUFFERED)return false;
    // Keep original near/fog clipping. Visibility is recomputed on every use;
    // only immutable packed attributes/topology survive camera movement.
    uint8_t slots[32],indices[192],remap[128];memset(remap,255,sizeof remap);unsigned vertices=0,out=0;
    for(unsigned i=0;i<triangle_count;i++) {
        const auto &t=triangles[i];unsigned ids[]={t.a,t.b,t.c};
        auto *a=&rsp.loaded_vertices[t.a],*b=&rsp.loaded_vertices[t.b],*c=&rsp.loaded_vertices[t.c];
        for(unsigned id:ids) {
            const auto &v=rsp.loaded_vertices[id];
            if(!(v.w>0 && v.z+v.w>1e-4f))return false;
            if(native_distance_fog_active && !(native_distance_fog.end_ndc*v.w-v.z>0))return false;
        }
        bool hidden=(a->clip_rej & b->clip_rej & c->clip_rej)!=0;
        if(native_baked_packet && ((a->x==b->x && a->y==b->y && a->z==b->z && a->w==b->w) ||
           (a->x==c->x && a->y==c->y && a->z==c->z && a->w==c->w) ||
           (b->x==c->x && b->y==c->y && b->z==c->z && b->w==c->w)))hidden=true;
        if(!hidden && (rsp.geometry_mode&G_CULL_BOTH)) {
            float cross=native_triangle_facing(a->x,a->y,a->w,b->x,b->y,b->w,c->x,c->y,c->w);
            unsigned mode=rsp.geometry_mode&G_CULL_BOTH;
            hidden=mode==G_CULL_BOTH || (mode==G_CULL_FRONT && cross<=0) || (mode==G_CULL_BACK && cross>=0);
        }
        for(unsigned id:ids) {
            unsigned j=remap[id];
            if(j==255){if(vertices==32)return false;j=vertices;remap[id]=j;slots[vertices++]=id;}
        }
        if(hidden)continue;
        for(unsigned id:ids) {
            indices[out++]=remap[id];
        }
    }
    if(!out) {
        if(native_rsp_profile::enabled) {
            native_batch_triangles+=triangle_count;native_batch_rejected+=triangle_count;
            if(native_scenery_kind==2){native_object_fast+=triangle_count;native_object_rejected+=triangle_count;}
            if(native_distance_fog_active)native_distance_seen+=triangle_count;
        }
        return true;
    }
    auto note=[&]() {
        if(native_rsp_profile::enabled) {
            native_batch_triangles+=triangle_count;native_batch_rejected+=triangle_count-out/3;
            if(native_scenery_kind==2){native_object_fast+=triangle_count;native_object_rejected+=triangle_count-out/3;}
            if(native_distance_fog_active)native_distance_seen+=triangle_count;
        }
    };
    auto fallback=[&]() {
        LoadedVertex *emit[192];for(unsigned i=0;i<out;i++)emit[i]=&rsp.loaded_vertices[slots[indices[i]]];
        unsigned done=0;
        while(done<out/3) {
            unsigned span=std::min(out/3-done,unsigned(MAX_BUFFERED-buf_vbo_num_tris));
            if(!span){gfx_flush();continue;}
            native_pack_triangles(emit+done*3,span,false);done+=span;
        }
        note();return true;
    };
    native_prepare_triangles(false);
    if(!native_scenery_context || native_triangle_setup.use_grayscale)return fallback();
    NativePreparedBatch::Attribute attributes[32];
    for(unsigned i=0;i<vertices;i++) {
        const auto &v=rsp.loaded_vertices[slots[i]];
        attributes[i]={v.u,v.v,v.color};
    }
    const auto &material=*native_scenery_cache.current_context;
    unsigned hash=native_scenery_cache.current_hash;
    for(unsigned i=0;i<vertices;i++) {
        hash=(hash^slots[i])*16777619u;
        uint32_t words[3];static_assert(sizeof words==sizeof attributes[i]);memcpy(words,&attributes[i],sizeof words);
        for(unsigned word:words)hash=(hash^word)*16777619u;
    }
    hash^=hash>>16;
    int found_index=native_prepared_index.find(hash,[&](unsigned i) {
        const auto &candidate=native_prepared_batches[i];
        return candidate.context && candidate.vertices==vertices && !memcmp(&candidate.material,&material,sizeof material) &&
            !memcmp(candidate.slots,slots,vertices) && !memcmp(candidate.attributes,attributes,vertices*sizeof attributes[0]);
    });
    unsigned selected_index=found_index>=0?unsigned(found_index):native_prepared_index.victim();
    auto *found=found_index>=0?&native_prepared_batches[found_index]:nullptr;
    auto *selected=&native_prepared_batches[selected_index];
    if(!selected){native_prepared_inflight++;return fallback();}
    auto &entry=*selected;
    bool hit=found!=nullptr;
    if(!found)native_prepared_key_miss++;
    if(hit)for(unsigned i=0;i<out;i++)if(!entry.ready[indices[i]]){hit=false;native_prepared_partial++;break;}
    if(entry.live_epoch==native_reference_epoch){native_prepared_inflight++;return fallback();}
    if(!hit && native_prepared_builds>=16){native_prepared_limit++;return fallback();}
    unsigned lod_rgb=0,lod_alpha=0;
    if(rdp.other_mode_h&G_TL_LOD)for(unsigned j=0;j<native_triangle_setup.num_inputs;j++) {
        if(native_triangle_setup.comb->shader_input_mapping[0][j]==G_CCMUX_LOD_FRACTION)lod_rgb|=1u<<j;
        if(native_triangle_setup.use_alpha && native_triangle_setup.comb->shader_input_mapping[1][j]==G_CCMUX_LOD_FRACTION)lod_alpha|=1u<<j;
    }
    if(buf_vbo_num_tris && (lod_rgb!=native_batch_lod_rgb || lod_alpha!=native_batch_lod_alpha))gfx_flush();
    // Preserve the original VBO boundaries; boundary-spanning batches use
    // the existing split/flush path instead of introducing extra draws.
    if(buf_vbo_num_tris+out/3>=MAX_BUFFERED){native_prepared_capacity++;return fallback();}
    unsigned start_corner=buf_vbo_num_tris*3,start_float=buf_vbo_len;
    if(!hit) {
        LoadedVertex *emit[192];for(unsigned i=0;i<out;i++)emit[i]=&rsp.loaded_vertices[slots[indices[i]]];
        native_pack_triangles(emit,out/3,false);
        // <=64 triangles cannot reach the reference 256-triangle auto flush.
        unsigned stride=(buf_vbo_len-start_float)/out;
        if(stride>40 || stride*out!=buf_vbo_len-start_float)return true;
        if(!found) {
            if(entry.context)native_prepared_replaced++;
            memset(entry.ready,0,sizeof entry.ready);entry.material=material;
        }
        entry.context=native_scenery_context;entry.vertices=vertices;entry.triangles=out/3;entry.stride=stride;
        memcpy(entry.slots,slots,vertices);memcpy(entry.indices,indices,out);
        memcpy(entry.attributes,attributes,vertices*sizeof attributes[0]);
        bool copied[32]={};
        for(unsigned i=0;i<out;i++) {
            unsigned j=indices[i];if(copied[j])continue;copied[j]=true;memcpy(entry.data[j],native_vertex_refs[start_corner+i],stride*sizeof(float));
            entry.attribute_ids[j]=native_attribute_ids[start_corner+i];entry.ready[j]=true;
        }
        entry.lod_rgb=native_batch_lod_rgb;entry.lod_alpha=native_batch_lod_alpha;
        native_prepared_builds++;native_prepared_misses++;
    }else {
        auto &setup=native_triangle_setup;
        unsigned texture_floats=0;
        for(unsigned t=0;t<2;t++)if(setup.used_textures[t])texture_floats+=2+((setup.tm>>(2*t))&1)+((setup.tm>>(2*t+1))&1);
        unsigned fog_offset=4+texture_floats,channels=setup.use_alpha?4:3;
        unsigned input_offset=fog_offset+(setup.use_fog?4:0);
        for(unsigned i=0;i<vertices;i++) {
            if(!entry.ready[i])continue;
            const auto &v=rsp.loaded_vertices[slots[i]];float *p=entry.data[i];
            p[0]=v.x;p[1]=setup.clip_parameters.invert_y?-v.y:v.y;
            p[2]=setup.clip_parameters.z_is_from_0_to_1?(v.z+v.w)/2.f:v.z;p[3]=v.w;
            if(setup.use_fog)p[fog_offset+3]=v.fog_n/255.f;
            if(entry.lod_rgb || entry.lod_alpha) {
                const float distance_frac=std::max(0.f,std::min(v.w/1024.f,1.f));
                const uint8_t lod=(.7f+distance_frac*.3f)*255.f;
                float value=native_unorm8(lod);
                for(unsigned j=0;j<setup.num_inputs;j++) {
                    if(entry.lod_rgb&(1u<<j))for(unsigned k=0;k<3;k++)p[input_offset+j*channels+k]=value;
                    if(entry.lod_alpha&(1u<<j))p[input_offset+j*channels+3]=value;
                }
            }
        }
        float fog_colour[3]={};native_batch_metadata(fog_colour);
        for(unsigned i=0;i<out;i++) {
            unsigned j=indices[i];native_vertex_refs[start_corner+i]=entry.data[j];
            native_vertex_ids[start_corner+i]=(uint64_t(native_packed_generation)<<8)|(slots[j]+1);
            native_attribute_ids[start_corner+i]=entry.attribute_ids[j];
        }
        buf_vbo_num_tris+=out/3;buf_vbo_len+=out*entry.stride;entry.live_epoch=native_reference_epoch;
        native_prepared_hits++;native_prepared_corners+=out;
    }
    if(!found)native_prepared_index.install(selected_index,hash);
    native_prepared_index.touch(selected_index);
    entry.last_used=++native_prepared_clock;
    note();
    return true;
}
'''
    text=text.replace(needle,helper+'\n'+needle)
    needle='    native_rsp_profile::Scope profile(native_rsp_profile::batch);'
    assert text.count(needle)==1
    # The normal CPU loop already owns boundary handling and exact flushes.
    # Probe safe spans there, leaving special vertices on that original path.
    a=text.index('    native_set_gpu_world(nullptr);\n    for(unsigned i=0;i<triangle_count;i++) {')
    loop=text.index('    for(unsigned i=0;i<triangle_count;i++) {',a)
    text=text[:loop]+'    unsigned native_probe_until=0;\n    static const bool native_probe_disabled=getenv("EOG_DISABLE_PREPARED_BATCHES")!=nullptr;\n'+text[loop:]
    at=text.index('\n',text.index('    for(unsigned i=0;i<triangle_count;i++) {',loop))+1
    text=text[:at]+'''        unsigned safe_end=i;
        if(!native_probe_disabled && i>=native_probe_until && !native_gpu_world_available() && native_reference_enabled()) {
            while(safe_end<triangle_count && safe_end-i<64) {
                const auto &candidate=triangles[safe_end];bool safe=true;
                for(unsigned id:{unsigned(candidate.a),unsigned(candidate.b),unsigned(candidate.c)}) {
                    const auto &v=rsp.loaded_vertices[id];
                    safe=safe && v.w>0 && v.z+v.w>1e-4f && (!native_distance_fog_active || native_distance_fog.end_ndc*v.w-v.z>0);
                }
                if(!safe)break;
                safe_end++;
            }
            if(safe_end-i>=2) {
                submit();
                if(native_replay_batch(triangles+i,safe_end-i)){i=safe_end-1;continue;}
                native_probe_until=safe_end;
            }else if(safe_end==i)native_prepared_boundary++;
        }
'''+text[at:]
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'\n    native_prepared_hits=native_prepared_misses=native_prepared_corners=native_prepared_builds=native_prepared_replaced=0;\n    native_prepared_key_miss=native_prepared_partial=native_prepared_inflight=native_prepared_limit=native_prepared_capacity=native_prepared_boundary=0;')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Prepared world batches: hits=%u builds=%u reusedCorners=%u replaced=%u bytes=%u",native_prepared_hits,native_prepared_misses,native_prepared_corners,native_prepared_replaced,(unsigned)sizeof native_prepared_batches);
'''+needle)
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Prepared exclusions: keyMiss=%u partial=%u inFlight=%u buildLimit=%u capacity=%u boundary=%u",native_prepared_key_miss,native_prepared_partial,native_prepared_inflight,native_prepared_limit,native_prepared_capacity,native_prepared_boundary);
'''+needle)
    # Host-only stress: recycle material IDs between frames without changing
    # material content, so token-dependent keys cannot accidentally pass.
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'''
#ifndef __3DS__
    if(getenv("EOG_TEST_PREPARED_CHURN"))memset(native_scenery_cache.materials,0,sizeof native_scenery_cache.materials);
#endif
''')
    return text
