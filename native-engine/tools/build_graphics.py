#!/usr/bin/env python3
"""Build the first real Fast3D -> PICA graphics bring-up, separate from gameplay."""
import csv
import json
import shutil
import sys
from pathlib import Path
import zipfile
import build
import build_probe
from native_fixture import canonical_rom, fnv


def graphics_objects():
    arch=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft']
    out=build.BUILD/'graphics';out.mkdir(exist_ok=True)
    for name in ('native_clip','native_world'):
        shader=out/(name+'.shbin')
        build_probe.run(['/opt/devkitpro/tools/bin/picasso','-o',shader,build.TARGET/('src/'+name+'.v.pica')])
        data=shader.read_bytes()
        (out/(name+'_shbin.h')).write_text('#pragma once\nstatic const unsigned char '+name+'_shbin[] __attribute__((aligned(4))) = {'+','.join(str(b) for b in data)+'};\nstatic const unsigned '+name+'_shbin_size=sizeof '+name+'_shbin;\n')
    objects=[]
    for name in ('graphics_scene.c','gfx_native.cpp','gfx_pica.cpp','display_3ds.c','pica_inputs.cpp','native_room.c','native_motion.c','baseline_options.c'):
        obj=out/(name+'.o');cpp=name.endswith('.cpp')
        if name in ('gfx_pica.cpp','display_3ds.c','pica_inputs.cpp'):
            flags=[*arch,'-std=gnu++20' if cpp else '-std=gnu11','-O2',*(['-fno-rtti','-fno-exceptions'] if cpp else []),'-Wall','-Wextra','-Werror','-D__3DS__',
                   '-I'+str(build.TARGET/'include'),'-I'+str(out),'-I'+str(build.UP/'port/fast3d'),'-I/opt/devkitpro/libctru/include']
        else:
            flags=build_probe.engine_args()
            flags += ['-I'+str(build.UP/'port/fast3d')]
            if cpp:
                flags=[s for s in flags if s not in ('-std=gnu11','-fplan9-extensions','-fpermissive')]
                flags += ['-std=gnu++20','-fno-rtti','-fno-exceptions']
        build_probe.run([build.CC.with_name('arm-none-eabi-g++') if cpp else build.CC,*flags,'-c',build.TARGET/'src'/name,'-o',obj])
        objects.append(obj)
    return objects


def main():
    build.prepare()
    # A release is packaged only after both original-code and full interpreter
    # host checks pass. These are CPU checks, not evidence of a working GPU.
    core=build_probe.run([sys.executable,build.TARGET/'tools/build_probe.py','--host-only'],timeout=60)
    (build.BUILD/'graphics-core-validation.log').write_text(core.stdout+core.stderr)
    import check_graphics_host
    check_graphics_host.main()
    import check_display_host
    check_display_host.main()
    # Refresh the generated Fast3D overlay and library without rebuilding game C.
    rows=[build.compile_one(Path('port/fast3d')/(n+'.cpp')) for n in ('gfx_pc','gfx_cc')]
    if not all(r['ok'] for r in rows):raise RuntimeError(str(rows))
    build_probe.run([build.CC.with_name('arm-none-eabi-ar'),'rcs',build.BUILD/'liboriginal-rsp.a',*[build.TARGET/r['object'] for r in rows]])
    (build.BUILD/'graphics-compile-report.json').write_text(json.dumps(rows,indent=2)+'\n')
    target=build_probe.native_runner(graphics=True)
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    with (build.UP/'scripts/filelist.u.csv').open() as f:
        row=next(r for r in csv.reader(f) if 'usedby7F008DE4.bin' in r[2])
    start,size=map(int,row[:2]);encoded=bytes(rom[start:start+size])
    if encoded[:4]!=b'\x01\xb8\x01\x2b':raise ValueError('Unexpected original menu dimensions')
    (build.BUILD/'native-menu.rle').write_bytes(encoded)
    sd=build.ROOT/'local/native-sd/3ds/eyes-of-gold-native';sd.mkdir(parents=True,exist_ok=True)
    files=[target.with_suffix('.3dsx'),target.with_suffix('.smdh'),build.BUILD/'native-menu.rle',build.BUILD/'native-probe.dat',build.BUILD/'native-assets.dat',build.BUILD/'native-room.dat',build.BUILD/'native-world.dat',build.BUILD/'native-motion.dat']
    for p in files:shutil.copy2(p,sd/p.name)
    for n in ('LICENSE','NOTICE'):shutil.copy2(build.UP/n,sd/('upstream-'+n+'.txt'))
    (sd/'GRAPHICS-README.txt').write_text('Eyes of Gold: Native Dam walking test. Not complete gameplay.\nExtract at SD root, then launch Eyes of Gold: Graphics.\nOriginal-code checks run first, followed by original Dam rooms through native Fast3D/PICA.\nCircle Pad: move. C-stick: look. L/R: turn. D-pad up/down: look. Y: reset. X: Dam. A: backdrop. B: triangles. START: exit.\nKeep all DAT/RLE files alongside the executable.\nOriginal compressed BG streams, texture codecs, STAN collision, gait animations and head filters. Conservative portal adjacency visibility; original Bond movement and full portal clipping are not integrated yet.\nNo props, doors, guards, weapons or missions in this test.\nROM-derived files came from the verified local US ROM. Recreation and text diagnostic remain separate.\n')
    archive=build.ROOT/'local/releases/eyes-of-gold-native-graphics.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for n in [p.name for p in files]+['upstream-LICENSE.txt','upstream-NOTICE.txt','GRAPHICS-README.txt']:
            z.write(sd/n,'3ds/eyes-of-gold-native/'+n)
    print('Original menu RLE:',len(encoded),'bytes; FNV',fnv(encoded))
    print('Graphics bring-up:',archive)

if __name__=='__main__':main()
