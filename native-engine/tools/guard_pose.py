"""Exact, content-keyed animation decode and local-rotation reuse."""
def apply(text):
    signature='void sub_GAME_7F06DEC0(s32 jointnum, s32 flip, ModelSkeleton *skeleton, ModelAnimation *anim, u8 *bitstream, coord3d *rot)'
    at=text.index('void modelBuildGroupMatrices(')
    text=text[:at]+'#include "native_guard_pose.h"\n'+text[at:]
    # Restrict matrix replacement to local transforms in this model routine.
    a=text.index('void modelBuildGroupMatrices(');b=text.index('\nvoid ',a+5)
    body=text[a:b];assert body.count('matrix_4x4_set_position_and_rotation_around_xyz(')==2
    text=text[:a]+body.replace('matrix_4x4_set_position_and_rotation_around_xyz(','native_guard_local_matrix(')+text[b:]
    a=text.index(signature);b=text.index('\n\nvoid process_02_position',a)
    body=text[a:b]
    needle='    rotation[0] = modelAnimReadBitsAsU16Angle(bitstream, width, bitoffset);'
    assert body.count(needle)==1
    body=body.replace(needle,'''    uint64_t native_bits=0;unsigned native_key=0;
    struct NativeGuardRotationEntry *native_entry=native_guard_rotation_slot(bitoffset,width,flip,bitstream,&native_bits,&native_key);
    if(native_entry && native_entry->key==native_key && native_entry->bits==native_bits){*rot=native_entry->value;native_guard_rotation_hits++;return;}
    native_guard_rotation_misses++;
'''+needle)
    last=body.rindex('}')
    body=body[:last]+'''    if(native_entry){native_entry->bits=native_bits;native_entry->key=native_key;native_entry->value=*rot;}
'''+body[last:]
    return text[:a]+body+text[b:]

def profile(text):
    text='#include "native_guard_profile.h"\n'+text
    def bounds(signature):
        a=text.index(signature);b=text.index('{',a);end=b+1;depth=1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        return a,end
    a,b=bounds('s32 chrTick(PropRecord *prop)')
    body=text[a:b].replace('s32 chrTick(', 'static s32 native_chr_tick_impl(',1)
    for old,new in [('chrlvActionTick(chr);','NATIVE_GUARD_TIME(0,chrlvActionTick(chr));'),('chrUpdateAnim(chr, tickamount);','NATIVE_GUARD_TIME(1,chrUpdateAnim(chr, tickamount));'),('subcalcmatrices(&renderdata, model);','NATIVE_GUARD_TIME(2,subcalcmatrices(&renderdata, model));')]:
        assert old in body;body=body.replace(old,new)
    old='        chr->field_20 = sub_GAME_7F06B120(NULL, model);';assert body.count(old)==1
    body=body.replace(old,'        uint64_t native_lists_start=native_guard_begin();\n'+old)
    old='        chr->field_20 = sub_GAME_7F06BB28(chr->field_20);';assert body.count(old)==1
    body=body.replace(old,old+'\n        native_guard_end(3,native_lists_start);')
    body+='''
s32 chrTick(PropRecord *prop) {
    native_guard_sample=(++native_guard_tick_calls%67)==0;
    uint64_t start=native_guard_begin();
    s32 result=native_chr_tick_impl(prop);
    native_guard_end(4,start);native_guard_sample=0;return result;
}
'''
    text=text[:a]+body+text[b:]
    a,b=bounds('Gfx *chrRenderProp(PropRecord *prop, Gfx *gdl, s32 withalpha)')
    body=text[a:b].replace('Gfx *chrRenderProp(', 'static Gfx *native_chr_render_impl(',1)
    body+='''
Gfx *chrRenderProp(PropRecord *prop,Gfx *gdl,s32 withalpha) {
    native_guard_sample=(++native_guard_render_calls%67)==0;
    uint64_t start=native_guard_begin();
    Gfx *result=native_chr_render_impl(prop,gdl,withalpha);
    native_guard_end(5,start);native_guard_sample=0;return result;
}
'''
    return text[:a]+body+text[b:]
