"""Carry original camera-LOD input semantics across the synchronous RSP draw."""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    replace('static void gfx_flush(void) {','''extern "C" void native_pica_lod_hint(unsigned,unsigned) __attribute__((weak));
extern "C" void native_pica_input_hint(unsigned,unsigned) __attribute__((weak));
static unsigned native_batch_lod_rgb,native_batch_lod_alpha;
static unsigned native_batch_uniform,native_batch_alpha_uniform,native_batch_proof_context;
static void gfx_flush(void) {''')
    needle='        gfx_rapi->draw_triangles(buf_vbo, buf_vbo_len, buf_vbo_num_tris);'
    replace(needle,'''        if(native_pica_input_hint)native_pica_input_hint(native_batch_uniform,native_batch_alpha_uniform);
        if(native_pica_lod_hint)native_pica_lod_hint(native_batch_lod_rgb,native_batch_lod_alpha);
'''+needle+'''
        if(native_pica_lod_hint)native_pica_lod_hint(0,0);
        if(native_pica_input_hint)native_pica_input_hint(0,0);''')
    needle='    float native_fog_colour[3]={};'
    replace(needle,'''    unsigned lod_rgb=0,lod_alpha=0;
    if(native_scenery_active && (rdp.other_mode_h & G_TL_LOD))for(unsigned j=0;j<num_inputs;j++) {
        if(comb->shader_input_mapping[0][j]==G_CCMUX_LOD_FRACTION)lod_rgb|=1u<<j;
        if(use_alpha && comb->shader_input_mapping[1][j]==G_CCMUX_LOD_FRACTION)lod_alpha|=1u<<j;
    }
    if(buf_vbo_num_tris && (lod_rgb!=native_batch_lod_rgb || lod_alpha!=native_batch_lod_alpha))gfx_flush();
    native_batch_lod_rgb=lod_rgb;native_batch_lod_alpha=lod_alpha;
    unsigned uniform=0,alpha_uniform=0;
    if(native_scenery_active && native_scenery_context)for(unsigned j=0;j<num_inputs;j++) {
        bool varying[2]={false,false};
        for(unsigned k=0;k<1u+use_alpha;k++) {
            unsigned source=comb->shader_input_mapping[k][j];
            varying[k]=source==G_CCMUX_SHADE || source==G_CCMUX_SHADE_ALPHA ||
                (source==G_CCMUX_LOD_FRACTION && (rdp.other_mode_h&G_TL_LOD));
        }
        if(!varying[0] && !varying[1])uniform|=1u<<j;
        if(use_alpha && !varying[1])alpha_uniform|=1u<<j;
    }
    if(!buf_vbo_num_tris) {
        native_batch_proof_context=native_scenery_context;
        native_batch_uniform=uniform;native_batch_alpha_uniform=alpha_uniform;
    }else {
        // A different source/context can carry different primitive/env values
        // in the same draw. Invalidate proofs instead of splitting/reordering it.
        if(!native_scenery_context || native_batch_proof_context!=native_scenery_context)uniform=alpha_uniform=0;
        native_batch_uniform&=uniform;native_batch_alpha_uniform&=alpha_uniform;
    }

'''+needle)
    return text
