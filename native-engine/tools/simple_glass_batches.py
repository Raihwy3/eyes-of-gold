"""Ordered reusable GPU batches for explicitly scoped flat-tint panes only."""
def apply(text):
    def replace(a,b):
        nonlocal text
        assert text.count(a)==1,a
        text=text.replace(a,b)
    # The separate scope is never used for reflective originals, mixed doors,
    # broken panes or other transparent effects.
    replace('native_scenery_kind==1 || ((native_scenery_kind==2 || native_scenery_kind==4 || ((native_scenery_kind==3 || native_scenery_kind==6) && !getenv("EOG_DISABLE_ACTOR_BATCHES"))) && native_object_batch_enabled())',
      'native_scenery_kind==1 || ((native_scenery_kind==2 || native_scenery_kind==4 || ((native_scenery_kind==3 || native_scenery_kind==6) && !getenv("EOG_DISABLE_ACTOR_BATCHES")) || (native_scenery_kind==5 && native_glass_detail==1)) && native_object_batch_enabled())')
    old='!(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE|G_NO_CLIPPING_EXT))'
    replace(old,'!(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|G_NO_CLIPPING_EXT)) &&\n        (!(rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE) || (native_scenery_kind==5 && native_glass_detail==1))')
    old='(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE|G_NO_CLIPPING_EXT)))return 0;'
    replace(old,'(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|G_NO_CLIPPING_EXT)) ||\n       ((rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE) && !(native_scenery_kind==5 && native_glass_detail==1)))return 0;')
    replace('static unsigned native_run_scenery_batch(Gfx *cmd) {','''static unsigned native_run_scenery_batch(Gfx *cmd) {
    bool simple_glass=native_scenery_kind==5 && native_glass_detail==1 && (rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE);
    NativeGlassMaterial simple_glass_material(simple_glass);''')
    replace('!(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE))) {',
      '!(rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE)) &&\n           (!(rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE) || (native_scenery_kind==5 && native_glass_detail==1))) {')
    replace('''    bool gpu=native_gpu_world_available() && triangle_count &&
        (rdp.other_mode_l & Z_UPD) && !(rdp.other_mode_l & CVG_X_ALPHA) &&
        (rdp.other_mode_l & ZMODE_DEC)!=ZMODE_DEC && !(rdp.other_mode_l & G_ZS_PRIM) &&
        !(rsp.geometry_mode & G_TEXTURE_GEN);''','''    bool gpu=native_gpu_world_available() && triangle_count &&
        (simple_glass || ((rdp.other_mode_l & Z_UPD) && !(rdp.other_mode_l & CVG_X_ALPHA) &&
        (rdp.other_mode_l & ZMODE_DEC)!=ZMODE_DEC && !(rdp.other_mode_l & G_ZS_PRIM) &&
        !(rsp.geometry_mode & G_TEXTURE_GEN)));''')
    replace('native_gpu_world_triangles++;native_batch_triangles++;','native_gpu_world_triangles++;native_batch_triangles++;if(simple_glass){native_glass_triangles++;native_glass_gpu_triangles++;}')
    replace('    native_set_texture_lod(tier);\n    const NativeMeshLod::Triangle *triangles=',
      '    if(simple_glass)tier=native_glass_lod;\n    native_set_texture_lod(native_effect_texture_tier?native_effect_texture_tier-1:tier);\n    const NativeMeshLod::Triangle *triangles=')
    replace('''    if(!native_scenery_active || is_rect || (rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE|NATIVE_GLASS_SCOPE)))native_set_texture_lod(0);''',
      '''    if(native_effect_texture_tier)native_set_texture_lod(native_effect_texture_tier-1);
    else if(rsp.extra_geometry_mode & NATIVE_GLASS_SCOPE)native_set_texture_lod(native_glass_lod);
    else if(!native_scenery_active || is_rect || (rsp.extra_geometry_mode & (NATIVE_WATCH_SCOPE|NATIVE_VIEWMODEL_SCOPE)))native_set_texture_lod(0);''')
    replace('            case NATIVE_SCENERY_COMMAND:','''            case NATIVE_GLASS_DETAIL_COMMAND:
                gfx_flush();native_glass_detail=C0(0,1);native_glass_lod=C0(1,2);
                native_triangle_setup_dirty=true;native_invalidate_packed_vertices();
                break;
            case NATIVE_SCENERY_COMMAND:''')
    return text
