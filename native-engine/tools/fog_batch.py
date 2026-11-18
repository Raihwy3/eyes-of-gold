"""Prove uniform fog RGB while packing, avoiding repeated backend RGB scans.

Mixed-colour batches retain the original validator and batch boundaries.
The hint is scoped to one synchronous backend call; no GPU state changes.
"""
def apply(text):
    needle='static void gfx_flush(void) {'
    assert text.count(needle)==1
    text=text.replace(needle,'''extern "C" void native_pica_fog_hint(int,const float *) __attribute__((weak));
static bool native_batch_fog_uniform;
static unsigned native_batch_fog_rgb;
static float native_batch_fog_colour[3];
'''+needle)
    needle='        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);'
    assert text.count(needle)==1
    text=text.replace(needle,'''        if(native_pica_fog_hint)native_pica_fog_hint(native_batch_fog_uniform,native_batch_fog_colour);
'''+needle+'''
        if(native_pica_fog_hint)native_pica_fog_hint(0,nullptr);
''')
    begin=text.index('        if (use_fog) {',text.index('const int n_emit_verts'))
    end=text.index('        if (use_grayscale)',begin)
    block=text[begin:end]
    compute=block[block.index('            unsigned r='):block.index('            buf_vbo[')]
    needle='    for (int i = 0; i < n_emit_verts; i++) {'
    assert text.count(needle)==1
    text=text.replace(needle,'''    float native_fog_colour[3]={};
    if(use_fog) {
'''+compute+'''        native_fog_colour[0]=native_unorm8(r);native_fog_colour[1]=native_unorm8(g);native_fog_colour[2]=native_unorm8(b);
        unsigned rgb=(r<<16)|(g<<8)|b;
        if(!buf_vbo_num_tris) {
            native_batch_fog_uniform=true;native_batch_fog_rgb=rgb;
            memcpy(native_batch_fog_colour,native_fog_colour,sizeof native_fog_colour);
        }else if(rgb!=native_batch_fog_rgb)native_batch_fog_uniform=false;
    }else native_batch_fog_uniform=false;
'''+needle)
    replacement='''        if (use_fog) {
            buf_vbo[buf_vbo_len++] = native_fog_colour[0];
            buf_vbo[buf_vbo_len++] = native_fog_colour[1];
            buf_vbo[buf_vbo_len++] = native_fog_colour[2];
            buf_vbo[buf_vbo_len++] = vi->fog_n / 255.0f;
        }

'''
    text=text.replace(block,replacement)
    return text
