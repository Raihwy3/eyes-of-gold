"""Cache invariant Fast3D triangle preparation between state commands."""
def apply(text):
    signature='static void gfx_sp_tri1(uint8_t'
    start=text.index(signature)
    end=text.index('static inline void gfx_sp_tri4',start)
    body=text[start:end]
    mode='    const uint32_t native_render_mode=native_aa_render_mode(rdp.other_mode_l,native_setting_aa());\n'
    assert body.count(mode)==1
    body=body.replace(mode,'')
    first=body.index('    bool depth_test =')
    last=body.index('    if (tri_dbg()) {   /* D474')
    prep=body[first:last]
    fields={
      'bool':['use_alpha','use_fog','use_grayscale','use_modulate'],
      'uint32_t':['tm'],
      'ColorCombiner*':['comb'],
      'uint8_t':['num_inputs'],
      'GfxClipParameters':['clip_parameters'],
      'uint32_t@2':['tex_width','tex_height','tex_width2','tex_height2'],
      'bool@2':['wrap_s','wrap_t','used_textures'],
      'float@2':['wrap_tw','wrap_th','wrap_uls','wrap_ult'],
    }
    decl=[];aliases=[]
    for kind,names in fields.items():
      base,_,size=kind.partition('@')
      for name in names:
        decl.append(f'    {base} {name}'+(f'[{size}]' if size else '')+';')
        aliases.append(f'    auto &{name}=native_triangle_setup.{name};')
    for old,new in [
      ('bool use_alpha =','use_alpha ='),('const bool use_fog =','use_fog ='),
      ('const bool use_grayscale =','use_grayscale ='),('const bool use_modulate =','use_modulate ='),
      ('ColorCombiner* comb =','comb ='),('uint32_t tm = 0;','tm = 0;'),
      ('uint32_t tex_width[2], tex_height[2], tex_width2[2], tex_height2[2];',''),
      ('bool  wrap_s[2] = { false, false }, wrap_t[2] = { false, false };','wrap_s[0]=wrap_s[1]=wrap_t[0]=wrap_t[1]=false;'),
      ('float wrap_tw[2] = { 0, 0 }, wrap_th[2] = { 0, 0 };','wrap_tw[0]=wrap_tw[1]=wrap_th[0]=wrap_th[1]=0;'),
      ('float wrap_uls[2] = { 0, 0 }, wrap_ult[2] = { 0, 0 };','wrap_uls[0]=wrap_uls[1]=wrap_ult[0]=wrap_ult[1]=0;'),
      ('uint8_t num_inputs;',''),('bool used_textures[2];',''),
      ('struct GfxClipParameters clip_parameters =','clip_parameters =')]:
        assert prep.count(old)==1,old
        prep=prep.replace(old,new)
    prefix='''// Invalidated by every state/unknown command, each frame, and rectangle helpers.
static bool native_triangle_setup_dirty=true;
static unsigned native_triangle_setup_hits,native_triangle_setup_builds;
static struct NativeTriangleSetup {
'''+ '\n'.join(decl)+ '\n} native_triangle_setup;\n'
    replacement='\n'.join(aliases)+'''
    static const bool disable_setup_cache=getenv("EOG_DISABLE_RSP_PREP_CACHE")!=nullptr;
    if(native_triangle_setup_dirty || is_rect || disable_setup_cache || tri_dbg()) {
'''+mode+prep+'''
        native_triangle_setup_dirty=is_rect;
        if(native_rsp_profile::enabled)native_triangle_setup_builds++;
    } else if(native_rsp_profile::enabled)native_triangle_setup_hits++;

'''
    body=body[:first]+replacement+body[last:]
    text=text[:start]+prefix+body+text[end:]
    old='        uint32_t opcode = cmd->words.w0 >> 24;'
    assert text.count(old)==1
    text=text.replace(old,old+'''
        // Only these commands cannot change prepared material/texture state.
        switch(opcode) {
          case (uint8_t)G_TRI1: case (uint8_t)G_TRI4:
          case (uint8_t)G_VTX: case (uint8_t)G_MTX: case (uint8_t)G_POPMTX:
          case (uint8_t)G_DL: case (uint8_t)G_ENDDL: break;
          default: native_triangle_setup_dirty=true;break;
        }
''')
    old='    gfx_rapi->start_frame();'
    assert text.count(old)==1
    text=text.replace(old,old+'''
    native_triangle_setup_dirty=true;
    native_triangle_setup_hits=native_triangle_setup_builds=0;
''')
    old='    const uint64_t perf_t1 = perf_t0 ? gfx_perf_now_ns() : 0;'
    assert text.count(old)==1
    text=text.replace(old,'''    if(native_rsp_profile::enabled)sysLogPrintf(LOG_NOTE,"RSP triangle setup: reused=%u rebuilt=%u",native_triangle_setup_hits,native_triangle_setup_builds);
'''+old)
    return text
