"""Record real old/new backend state using actual Citro3D inline builders."""
from pathlib import Path
import os
import build,build_probe

def main():
    world_shader=(build.TARGET/'src/native_world.v.pica').read_text()
    assert 'mov outpos.z, r1.z' in world_shader
    assert 'add r0.x, r1.z, -r1.w' not in world_shader, 'World depth must be mapped before float24 transformation'
    clip_shader=(build.TARGET/'src/native_clip.v.pica').read_text()
    assert 'mov outpos.z, inpos.z' in clip_shader
    assert 'add r0.x, inpos.z, -inpos.w' not in clip_shader
    out=build.BUILD/'pica-backend-host';out.mkdir(exist_ok=True)
    test=build.TARGET/'tests/pica_backend'
    sanitize=['-fsanitize=address,undefined','-g','-fno-omit-frame-pointer','--param=asan-globals=0'] if os.environ.get('EOG_PICA_SANITIZE') else []
    common=['-std=c++20','-O1','-fno-pie','-ffunction-sections','-fdata-sections',*sanitize,
      '-I'+str(test/'mock'),'-I'+str(build.TARGET/'include'),'-I'+str(build.UP/'port/fast3d'),'-I/opt/devkitpro/libctru/include']
    for reference in (True,False):
        binary=out/('reference' if reference else 'current')
        build_probe.run(['g++',*common,*(['-DBACKEND_REFERENCE'] if reference else []),test/'check.cpp',build.TARGET/'src/pica_inputs.cpp',build.UP/'port/fast3d/gfx_cc.cpp','-no-pie','-Wl,--gc-sections','-o',binary])
        print(build_probe.run([binary,out/'reference.bin']).stdout)
if __name__=='__main__':main()
