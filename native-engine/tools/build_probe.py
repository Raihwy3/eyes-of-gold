#!/usr/bin/env python3
"""Link and validate genuine original routines before full platform bring-up."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile
import build
from native_fixture import create
from package_elf import prepare_package_elf, validate_3dsx

ORIGINAL = ['src/game/padhalllv.c','src/game/initpathtablelinks.c',
            'src/game/chrai.c','src/game/chraidata.c','src/game/chrprop.c',
            'src/game/lv.c','src/game/decompress.c','src/game/zlib.c','port/src/random.c','src/memp.c','src/game/title2.c','src/game/initanitable.c','src/ramrom.c','port/src/libultra.c','port/src/mixer.c']

def run(args,**kwargs):
    result = subprocess.run([str(a) for a in args],capture_output=True,text=True,**kwargs)
    if result.returncode:
        raise RuntimeError(f'{args[0]} failed:\n{result.stdout}\n{result.stderr}')
    return result

def engine_args(host=False):
    includes = [p for p in build.INCLUDES if not host or 'libctru' not in str(p)]
    includes = [p for p in includes if p != build.BUILD/'include']
    includes += [build.BUILD/('host/include' if host else 'include')]
    flags = [f for f in build.FLAGS if not host or not f.startswith(('-march=','-mtune=','-mfloat-abi=','-mtp='))]
    if host:
        flags += ['-fno-pie']
    return [*flags,*['-D'+s for s in build.DEFS if not host or s != '__3DS__'],
            *['-I'+str(p) for p in includes],'-include',build.TARGET/'include/prelude.h',
            '-include','versioninfo.h']

def host_runner():
    dest = build.BUILD/'host'
    inc = dest/'include'
    inc.mkdir(parents=True,exist_ok=True)
    for name in ('string','stdlib','sched','pthread'):
        (inc/f'host{name}.h').write_text(f'#include "/usr/include/{name}.h"\n')
    builtin = run(['gcc','-print-file-name=include/stddef.h']).stdout.strip()
    (inc/'hoststddef.h').write_text(f'#include "{builtin}"\n')
    shutil.copyfile(build.BUILD/'include/versioninfo.h',inc/'versioninfo.h')
    shutil.copyfile(build.BUILD/'include/native_object_lods.h',inc/'native_object_lods.h')
    shutil.copyfile(build.BUILD/'include/native_bg_lods.h',inc/'native_bg_lods.h')
    shutil.copyfile(build.BUILD/'include/native_room_lods.h',inc/'native_room_lods.h')
    shutil.copyfile(build.BUILD/'include/native_room_packets_data.h',inc/'native_room_packets_data.h')
    objects = []
    for rel in ORIGINAL:
        obj = dest/(rel.replace('/','__')+'.o')
        system=['-I'+str(build.BUILD/'include/libultra-system')] if rel=='port/src/libultra.c' else []
        run(['gcc',*system,*engine_args(True),'-c',build.source_path(Path(rel)),'-o',obj])
        objects.append(obj)
    probe = dest/'engine_probe.o'
    run(['gcc',*engine_args(True),'-c',build.TARGET/'src/engine_probe.c','-o',probe])
    fixups=dest/'rom_fixups.o'
    run(['gcc',*engine_args(True),'-c',build.TARGET/'src/rom_fixups.c','-o',fixups])
    platform = dest/'platform_host.o'
    run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/platform_host.c','-o',platform])
    ram=dest/'native_ram.o'
    run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/native_ram.c','-o',ram])
    cart=dest/'native_cart.o'
    run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/native_cart.c','-o',cart])
    memory=dest/'game_memory.o'
    run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/game_memory.c','-o',memory])
    symbols=dest/'romassets.o'
    run(['gcc','-c',build.UP/'port/src/romassets_u.s','-o',symbols])
    runtime=dest/'runtime_symbols.o'
    run(['gcc','-c',build.runtime_symbols(),'-o',runtime])
    binary = dest/'original-engine-probe'
    # Above the original physical-offset range, below 2 GB for inherited s32
    # pointer paths. This host reference does not require mmap/mirror pages.
    run(['gcc','-no-pie','-Wl,--gc-sections,-Ttext-segment=0x02000000',platform,probe,ram,cart,memory,fixups,symbols,runtime,*objects,'-pthread','-lm','-o',binary])
    return binary

def native_runner(graphics=False):
    probe = build.BUILD/'engine_probe.o'
    run([build.CC,*engine_args(),'-c',build.TARGET/'src/engine_probe.c','-o',probe])
    platform = build.BUILD/('platform_graphics.o' if graphics else 'platform_3ds.o')
    arch = ['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft']
    run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__',
         *(['-DNATIVE_GRAPHICS'] if graphics else []),
         '-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-I/opt/devkitpro/libctru/include',
         '-c',build.TARGET/'src/platform_3ds.c','-o',platform])
    target = build.BUILD/('eyes-of-gold-native-graphics' if graphics else 'eyes-of-gold-native')
    extras=[]
    if graphics:
        from build_graphics import graphics_objects
        extras=graphics_objects()
    ram=build.BUILD/'native_ram.o'
    run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/native_ram.c','-o',ram])
    cart=build.BUILD/'native_cart.o'
    run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/native_cart.c','-o',cart])
    memory=build.BUILD/'game_memory.o'
    run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),
         '-c',build.TARGET/'src/game_memory.c','-o',memory])
    run([build.CC.with_name('arm-none-eabi-g++') if graphics else build.CC,*arch,'-specs=3dsx.specs','-L/opt/devkitpro/libctru/lib',
         '-Wl,--gc-sections,--no-enum-size-warning,-Map,'+str(target.with_suffix('.map')),platform,probe,ram,cart,memory,build.BUILD/'rom_fixups.o',build.BUILD/'romassets.o',build.BUILD/'runtime_symbols.o',
         *extras,'-Wl,--start-group',build.BUILD/'liboriginal-engine.a',build.BUILD/'libnative-os.a',build.BUILD/'libnative-mixer.a',build.BUILD/'libnative-baseline.a',*([build.BUILD/'liboriginal-rsp.a'] if graphics else []),'-Wl,--end-group',*(['-lcitro3d'] if graphics else []),'-lctru','-lm','-o',target.with_suffix('.elf')])
    import audit_original_data
    audit_original_data.require_clean_release(target.with_suffix('.elf'))
    run(['/opt/devkitpro/tools/bin/smdhtool','--create','Eyes of Gold: Graphics' if graphics else 'Eyes of Gold: Native',
         'Native graphics bring-up' if graphics else 'Original engine diagnostic (not playable)','Eyes of Gold',build.ROOT/'icon.png',target.with_suffix('.smdh')])
    package = target.with_suffix('.package.elf')
    tokens = prepare_package_elf(target.with_suffix('.elf'), package)
    print('Fixed N64 tokens excluded from pointer relocations:', tokens)
    run(['/opt/devkitpro/tools/bin/3dsxtool',package,target.with_suffix('.3dsx'),
         '--smdh='+str(target.with_suffix('.smdh'))])
    print('Validated 3DSX relocations:', validate_3dsx(target.with_suffix('.3dsx')))
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,default=build.ROOT/'local/roms/GoldenEye.n64')
    parser.add_argument('--host-only',action='store_true')
    args = parser.parse_args()
    build.prepare()
    reference = host_runner()
    baseline_env={key:value for key,value in os.environ.items() if not key.startswith('GE_')}
    fixture = build.BUILD/'native-probe.dat'
    create(args.rom,build.UP,fixture)
    # Record a host reference, then require the complete probe to pass it.
    initial = subprocess.run([str(reference),str(fixture)],capture_output=True,text=True,timeout=60,env=baseline_env)
    rows = [line.split('\t',2) for line in initial.stdout.splitlines()]
    matches = [r for r in rows if len(r)==3 and r[2]=='Original pathfinder 84050 routes']
    if len(matches)!=1 or initial.returncode != 1 or any(r[0]=='FAIL' for r in rows if r != matches[0]):
        raise RuntimeError('Could not record original pathfinder reference:\n'+initial.stdout+initial.stderr)
    metadata = create(args.rom,build.UP,fixture,int(matches[0][1]))
    verified = run([reference,fixture],timeout=60,env=baseline_env)
    (build.BUILD/'host-validation.log').write_text(verified.stdout+verified.stderr)
    (build.BUILD/'fixture-manifest.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print(verified.stdout)
    if not args.host_only:
        target = native_runner()
        sd = build.ROOT/'local/native-sd/3ds/eyes-of-gold-native'
        sd.mkdir(parents=True,exist_ok=True)
        for path in (fixture,fixture.with_name('native-assets.dat'),target.with_suffix('.3dsx'),target.with_suffix('.smdh')):
            shutil.copy2(path,sd/path.name)
        for name in ('LICENSE','NOTICE'):
            shutil.copy2(build.UP/name,sd/('upstream-'+name+'.txt'))
        (sd/'README.txt').write_text('Original engine diagnostic; no gameplay yet.\nExtract at SD root; launch Eyes of Gold: Native.\nKeep native-probe.dat and native-assets.dat with the executable.\nThese small test data files are extracted from your local ROM.\nChecks run automatically; START exits. Results: native-engine.log.\nThe existing recreation is separate and unchanged.\n')
        archive = build.ROOT/'local/releases/eyes-of-gold-native-diagnostic.zip'
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
            for name in ('README.txt','eyes-of-gold-native.3dsx','eyes-of-gold-native.smdh',
                         'native-probe.dat','native-assets.dat','upstream-LICENSE.txt','upstream-NOTICE.txt'):
                path=sd/name
                z.write(path,path.relative_to(sd.parents[1]))
        print('Separate diagnostic:',archive)

if __name__ == '__main__':
    main()
