"""Render-only float fog, early far rejection and exact boundary clipping."""
def apply(text):
    needle='struct LoadedVertex {'
    at=text.index(needle)
    text=text[:at]+'''#include "native_distance_fog.h"
static bool native_distance_fog_active;
static NativeDistanceFog native_distance_fog;
static unsigned native_distance_seen,native_distance_hidden,native_distance_clipped;
'''+text[at:]
    needle='            case 0x46: /* G_HUDSCALE_EXT'
    at=text.index(needle)
    text=text[:at]+'''            case NATIVE_DISTANCE_FOG_COMMAND:
                gfx_flush();native_distance_fog_active=C0(0,1)!=0;
                if(native_distance_fog_active)native_distance_fog=*(const NativeDistanceFog *)seg_addr(cmd->words.w1);
                native_triangle_setup_dirty=true;native_invalidate_packed_vertices();
                break;
'''+text[at:]
    needle='    gfx_rapi->start_frame();'
    text=text.replace(needle,needle+'''
    native_distance_fog_active=false;
    native_distance_seen=native_distance_hidden=native_distance_clipped=0;
''')
    # Fog-active world loads need only the native fog ramp, not both ramps.
    a=text.index('static void gfx_sp_vertex(size_t');b=text.index('static void gfx_sp_tri1(',a)
    body=text[a:b]
    needle='        if (rsp.geometry_mode & G_FOG) {'
    assert body.count(needle)==1
    body=body.replace(needle,'''        static const bool native_fog_reference=getenv("EOG_DISABLE_FOG_FASTPATH")!=nullptr;
        if(native_distance_fog_active && !native_fog_reference) {
            d->fog=native_distance_fog_factor(&native_distance_fog,d->z,d->w);
            d->fog_n=gfx_fog_vertex?d->fog:d->fog*d->w;
        }else if (rsp.geometry_mode & G_FOG) {''')
    needle='        d->color.a = vcn->a;'
    assert body.count(needle)==1
    body=body.replace(needle,'''        if(native_distance_fog_active && native_fog_reference) {
            d->fog=native_distance_fog_factor(&native_distance_fog,d->z,d->w);
            d->fog_n=gfx_fog_vertex?d->fog:d->fog*d->w;
        }
'''+needle)
    text=text[:a]+body+text[b:]
    needle='        d->fog_n = gfx_fog_vertex ? d->fog : d->fog * w;'
    # The float-vertex extension is used by clip-space effects, too.
    a=text.index('static void gfx_sp_vertex_float(')
    b=text.index('static inline void *seg_addr(',a)
    body=text[a:b];assert body.count(needle)==1
    body=body.replace(needle,needle+'''
        if(native_distance_fog_active){d->fog=native_distance_fog_factor(&native_distance_fog,z,w);d->fog_n=gfx_fog_vertex?d->fog:d->fog*w;}
''')
    text=text[:a]+body+text[b:]
    needle='    use_fog = ((native_render_mode >> 30) == G_BL_CLR_FOG) || ((native_render_mode >> 26) == G_BL_A_FOG);'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'\n    if(native_distance_fog_active && !is_rect && (native_render_mode & (3U<<G_MDSFT_ALPHACOMPARE))!=G_AC_DITHER)use_fog=true;')
    needle='    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
    bool native_distance_crossing=false;
    if(native_distance_fog_active && !is_rect) {
        if(native_rsp_profile::enabled)native_distance_seen++;
        float end=native_distance_fog.end_ndc;
        float da=end*v1->w-v1->z,db=end*v2->w-v2->z,dc=end*v3->w-v3->z;
        if(da<=0 && db<=0 && dc<=0) {
            if(native_rsp_profile::enabled)native_distance_hidden++;
            return;
        }
        native_distance_crossing=da<=0 || db<=0 || dc<=0 ||
            ((rsp.extra_geometry_mode & G_NO_CLIPPING_EXT)==0 &&
             (v1->z+v1->w<=1e-4f || v2->z+v2->w<=1e-4f || v3->z+v3->w<=1e-4f));
    }
''')
    text=text.replace('struct LoadedVertex* emit_v[6]', 'struct LoadedVertex* emit_v[9]')
    needle='    if (nearclip_on && !is_rect && (rsp.extra_geometry_mode & G_NO_CLIPPING_EXT) == 0) {'
    assert text.count(needle)==1
    text=text.replace(needle,needle.replace('nearclip_on &&','!native_distance_fog_active && nearclip_on &&'))
    needle='    const int n_emit_verts = n_out_tris * 3;'
    assert text.count(needle)==1
    text=text.replace(needle,'''    LoadedVertex native_fog_corners[8];
    if(native_distance_fog_active && !is_rect && native_distance_crossing) {
        LoadedVertex *poly[8]={v1,v2,v3},*next[8];
        unsigned count=3,created=0;
        // Clip against parallel near/fog-end planes in homogeneous space.
        // Up to five polygon corners survive: never discard a crossing wall.
        for(unsigned plane=0;plane<2 && count;plane++) {
            if(!plane && (!nearclip_on || (rsp.extra_geometry_mode & G_NO_CLIPPING_EXT)))continue;
            unsigned n=0;
            for(unsigned i=0;i<count;i++) {
                LoadedVertex *a=poly[i],*b=poly[(i+1)%count];
                float da=plane?native_distance_fog.end_ndc*a->w-a->z:a->z+a->w-1e-4f;
                float db=plane?native_distance_fog.end_ndc*b->w-b->z:b->z+b->w-1e-4f;
                if(da>0)next[n++]=a;
                if((da>0)!=(db>0)) {
                    assert(created<8 && n<8);
                    float t=da/(da-db);LoadedVertex *o=&native_fog_corners[created++];
                    o->x=a->x+(b->x-a->x)*t;o->y=a->y+(b->y-a->y)*t;
                    o->z=a->z+(b->z-a->z)*t;o->w=a->w+(b->w-a->w)*t;
                    o->u=a->u+(b->u-a->u)*t;o->v=a->v+(b->v-a->v)*t;
                    uint8_t *oc=(uint8_t *)&o->color;
                    const uint8_t *ac=(const uint8_t *)&a->color,*bc=(const uint8_t *)&b->color;
                    for(unsigned c=0;c<4;c++)oc[c]=(uint8_t)clampf(floorf(ac[c]+(float(bc[c])-ac[c])*t+.5f),0.f,255.f);
                    o->clip_rej=0;o->fog=plane?255:native_distance_fog_factor(&native_distance_fog,o->z,o->w);
                    o->fog_n=gfx_fog_vertex?o->fog:o->fog*o->w;
                    next[n++]=o;
                }
            }
            count=n;for(unsigned i=0;i<count;i++)poly[i]=next[i];
        }
        if(count<3)return;
        if(created && native_rsp_profile::enabled)native_distance_clipped++;
        assert(count<=5);n_out_tris=count-2;
        for(unsigned i=0;i<count-2;i++){emit_v[i*3]=poly[0];emit_v[i*3+1]=poly[i+1];emit_v[i*3+2]=poly[i+2];}
    }
'''+needle)
    needle='            unsigned r=rdp.fog_color.r,g=rdp.fog_color.g,b=rdp.fog_color.b;'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''
            if(native_distance_fog_active){r=native_distance_fog.colour>>24;g=(native_distance_fog.colour>>16)&255;b=(native_distance_fog.colour>>8)&255;}
''')
    needle='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    text=text.replace(needle,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"Fog geometry: tested=%u hidden=%u clipped=%u",native_distance_seen,native_distance_hidden,native_distance_clipped);
'''+needle)
    return text
