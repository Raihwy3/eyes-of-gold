"""Hoist tile-invariant UV work out of per-vertex triangle packing.

Power-of-two operations remain exact. Non-power-of-two texture dimensions use
original division, preserving submitted UV bytes and the authored tile mapping.
"""
def apply(text):
    needle='static struct NativeTriangleSetup {\n'
    assert text.count(needle)==1
    text=text.replace(needle,needle+'''    float uv_shift[2][2],uv_origin[2][2],uv_inverse[2][2],uv_clamp[2][2];
    bool uv_power2[2][2],uv_half,uv_linear;
''')
    needle='        native_triangle_setup_dirty=is_rect;'
    assert text.count(needle)==1
    prep='''        auto &uv=native_triangle_setup;
        uv.uv_half=!(rdp.other_mode_h & G_TP_PERSP);
        uv.uv_linear=(rdp.other_mode_h & (3U<<G_MDSFT_TEXTFILT))!=G_TF_POINT;
        for(unsigned t=0;t<2;t++)if(used_textures[t]) {
            const auto &tile=rdp.texture_tile[rdp.first_tile_index+gfx_lod_tile_offset(t)];
            int shifts[2]={tile.shifts,tile.shiftt};
            unsigned sizes[2]={tex_width[t],tex_height[t]};
            unsigned bounds[2]={tex_width2[t],tex_height2[t]};
            uv.uv_origin[t][0]=tile.uls/4.f;uv.uv_origin[t][1]=tile.ult/4.f;
            for(unsigned a=0;a<2;a++) {
                int shift=shifts[a];
                uv.uv_shift[t][a]=shift<=10?1.f/(1u<<shift):float(1u<<(16-shift));
                unsigned size=sizes[a];
                uv.uv_power2[t][a]=size && !(size&(size-1));
                uv.uv_inverse[t][a]=size?1.f/size:0.f;
                uv.uv_clamp[t][a]=(bounds[a]-.5f)/size;
            }
        }
'''
    text=text.replace(needle,prep+needle)
    a=text.index('            // TODO: fix this; for now just ignore smaller mips',text.index('static void gfx_sp_tri1('))
    b=text.index('            // D74 (Video.WrapFix',a)
    old=text[a:b]
    new='''            static const bool native_uv_reference=getenv("EOG_DISABLE_UV_PREP")!=nullptr;
            float u,v;
            if(!native_uv_reference) {
                const auto &uv=native_triangle_setup;
                u=(vi->u/32.f)*uv.uv_shift[t][0]-uv.uv_origin[t][0];
                v=(vi->v/32.f)*uv.uv_shift[t][1]-uv.uv_origin[t][1];
            }else {
'''+old.replace('            float u =','            u =').replace('            float v =','            v =')+'''            }
'''
    text=text[:a]+new+text[b:]
    # Keep original divide as an executable reference and for NPOT textures.
    text=text.replace('            buf_vbo[buf_vbo_len++] = u / tex_width[t];','''            buf_vbo[buf_vbo_len++] = !native_uv_reference && native_triangle_setup.uv_power2[t][0] ? u*native_triangle_setup.uv_inverse[t][0] : u/tex_width[t];''')
    text=text.replace('            buf_vbo[buf_vbo_len++] = v / tex_height[t];','''            buf_vbo[buf_vbo_len++] = !native_uv_reference && native_triangle_setup.uv_power2[t][1] ? v*native_triangle_setup.uv_inverse[t][1] : v/tex_height[t];''')
    text=text.replace('(tex_width2[t] - 0.5f) / tex_width[t]','native_triangle_setup.uv_clamp[t][0]').replace('(tex_height2[t] - 0.5f) / tex_height[t]','native_triangle_setup.uv_clamp[t][1]')
    return text
