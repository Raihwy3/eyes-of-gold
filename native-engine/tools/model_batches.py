"""Opaque actor/vehicle batching and exact repeated-normal lighting reuse."""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    # No transformed vertices or illumination survive a vertex load. Reuse
    # only RGB results for identical normals under that load's fixed lights.
    start=text.index('            int r = rsp.current_lights[rsp.current_num_lights - 1].col[0];')
    end=text.index('\n\n            /* D195',start)
    lighting=text[start:end]
    text=text[:start]+'''            uint32_t normal_key=uint8_t(vcn->x)|(uint32_t(uint8_t(vcn->y))<<8)|(uint32_t(uint8_t(vcn->z))<<16);
            auto &normal_slot=normal_cache[(normal_key^(normal_key>>7)^(normal_key>>13))&31];
            if(!normal_reference && normal_slot.valid && normal_slot.key==normal_key) {
                d->color.r=normal_slot.rgb[0];d->color.g=normal_slot.rgb[1];d->color.b=normal_slot.rgb[2];
                if(native_rsp_profile::enabled)native_normal_reused++;
            }else {
'''+lighting+'''
                normal_slot.key=normal_key;normal_slot.valid=true;
                normal_slot.rgb[0]=d->color.r;normal_slot.rgb[1]=d->color.g;normal_slot.rgb[2]=d->color.b;
                if(native_rsp_profile::enabled)native_normal_built++;
            }'''+text[end:]
    replace('static void gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx* vertices) {','''static unsigned native_normal_reused,native_normal_built;
static void gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
    struct NormalSlot {uint32_t key;uint8_t rgb[3];bool valid;} normal_cache[32]{};
    static const bool normal_reference=getenv("EOG_DISABLE_LIGHT_NORMAL_CACHE")!=nullptr;''')
    # 3=opaque skinned actors, 4=opaque intact vehicles. Original 0/1/2
    # commands retain their encoding and meaning.
    replace('static unsigned native_scenery_kind;','''static unsigned native_scenery_kind;
static unsigned native_actor_triangles,native_actor_fallback,native_vehicle_triangles,native_vehicle_fallback;
static unsigned native_model_start_triangles,native_model_start_fallback;
static unsigned native_faded_triangles,native_faded_fallback;''')
    replace('                native_scenery_kind=C0(0,2);','''                if(native_scenery_kind==6){native_faded_triangles+=native_batch_triangles-native_model_start_triangles;native_faded_fallback+=native_batch_fallback-native_model_start_fallback;}
                if(native_scenery_kind==3 || native_scenery_kind==6){native_actor_triangles+=native_batch_triangles-native_model_start_triangles;native_actor_fallback+=native_batch_fallback-native_model_start_fallback;}
                if(native_scenery_kind==4){native_vehicle_triangles+=native_batch_triangles-native_model_start_triangles;native_vehicle_fallback+=native_batch_fallback-native_model_start_fallback;}
                native_scenery_kind=C0(0,3);
                native_model_start_triangles=native_batch_triangles;native_model_start_fallback=native_batch_fallback;''')
    replace('native_scenery_kind==1 || (native_scenery_kind==2 && native_object_batch_enabled())','native_scenery_kind==1 || ((native_scenery_kind==2 || native_scenery_kind==4 || ((native_scenery_kind==3 || native_scenery_kind==6) && !getenv("EOG_DISABLE_ACTOR_BATCHES"))) && native_object_batch_enabled())')
    # Preserve aggregate object counters while adding separate model census.
    text=text.replace('native_scenery_kind==2)', '(native_scenery_kind==2 || native_scenery_kind==4))')
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'''
    native_faded_triangles=native_faded_fallback=0;
    native_normal_reused=native_normal_built=0;
    native_actor_triangles=native_actor_fallback=native_vehicle_triangles=native_vehicle_fallback=0;
    native_model_start_triangles=native_model_start_fallback=0;
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Model batches: actorTriangles=%u actorFallback=%u vehicleTriangles=%u vehicleFallback=%u",native_actor_triangles,native_actor_fallback,native_vehicle_triangles,native_vehicle_fallback);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Guard faded batches: triangles=%u fallback=%u",native_faded_triangles,native_faded_fallback);
    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Lighting normals: reused=%u built=%u",native_normal_reused,native_normal_built);
'''+needle)
    return text
