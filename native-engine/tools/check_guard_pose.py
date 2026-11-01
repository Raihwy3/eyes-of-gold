"""Compare actual patched decoder/matrix helpers against retail C, with sanitizers."""
import os
from pathlib import Path
import build,build_probe

def function(text,signature):
    a=text.index(signature);end=text.index('{',a)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[a:end]

def main():
    out=build.BUILD/'guard-pose';out.mkdir(exist_ok=True)
    original=(build.UP/'src/game/model.c').read_text();patched=build.source_path(Path('src/game/model.c')).read_text()
    read=function(original,'u32 modelAnimReadBitsAsU16Angle(')
    original_decode=function(original,'void sub_GAME_7F06DEC0(').replace('sub_GAME_7F06DEC0(','reference_decode(',1)
    patched_decode=function(patched,'void sub_GAME_7F06DEC0(')
    (out/'pose-functions.h').write_text('#include "native_guard_pose.h"\n'+read+'\n'+original_decode+'\n'+patched_decode+'\n')
    flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
    objects=[]
    for source in (build.TARGET/'tests/guard_pose_host.c',build.UP/'src/game/matrixmath.c'):
        obj=out/(source.name+'.o');build_probe.run(['gcc',*build_probe.engine_args(True),*flags,'-I'+str(out),'-c',source,'-o',obj]);objects.append(obj)
    binary=out/'check';build_probe.run(['gcc',*flags,'-no-pie','-Wl,--gc-sections',*objects,'-lm','-o',binary])
    for disabled in (False,True):
        env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
        if disabled:env['EOG_DISABLE_GUARD_POSE_CACHE']='1'
        print(build_probe.run([binary],env=env).stdout)
if __name__=='__main__':
    build.prepare();build_probe.host_runner();main()
