#!/usr/bin/env python3
"""Build the original boss/game-loop boot target, never the graphics harness."""
from pathlib import Path
import sys
import json
from package_elf import prepare_package_elf,validate_3dsx
import shutil
import zipfile
import build
import build_probe
import build_graphics


def main():
    # The final game links these archives. Rebuild them here so header/overlay
    # changes cannot pass host checks while shipping an older ARM engine.
    engine=build_probe.run([sys.executable,build.TARGET/'tools/build.py','--jobs','4'])
    (build.BUILD/'compile-summary.log').write_text(engine.stdout+engine.stderr)
    print(engine.stdout,end='')
    build.prepare()
    import check_launcher_ui
    check_launcher_ui.main()
    import bottom_background
    bottom_background.generate()
    import native_game_assets,check_game_assets
    native_game_assets.main();check_game_assets.main()
    import audio_pcm_assets
    audio_pcm_assets.generate()
    import check_audio_pcm
    check_audio_pcm.main()
    import check_crosshair
    check_crosshair.main()
    import check_dsp_audio
    check_dsp_audio.main()
    import check_guard_pose
    check_guard_pose.main()
    import check_guard_activity
    check_guard_activity.main()
    import check_updater
    check_updater.main()
    import check_randomizer
    check_randomizer.main()
    import check_subsistence
    check_subsistence.main()
    import check_attract_camera
    check_attract_camera.main()
    subprocess_queue_check=build_probe.run([sys.executable,build.TARGET/"tools/check_os_queues.py"],timeout=45)
    print(subprocess_queue_check.stdout,end="")
    import check_work_profile
    check_work_profile.main()
    import check_effect_lod
    check_effect_lod.main()
    import check_multiplayer_preview
    check_multiplayer_preview.main()
    core=build_probe.run([sys.executable,build.TARGET/'tools/build_probe.py','--host-only'],timeout=60)
    (build.BUILD/'game-core-validation.log').write_text(core.stdout+core.stderr)
    import check_console_convert
    check_console_convert.main(original=True)
    import check_derived
    check_derived.main()
    import check_scenery_cache
    check_scenery_cache.main()
    import check_weapon_stereo
    check_weapon_stereo.main()
    import check_pica_backend
    check_pica_backend.main()
    import check_graphics_host,check_display_host
    check_graphics_host.main();check_display_host.main()
    out=build.BUILD/'game';out.mkdir(exist_ok=True)
    objects=[]
    for source in [build.TARGET/'src/native_game.c',build.TARGET/'src/native_randomizer.c',build.TARGET/'src/native_effects.c',build.TARGET/'src/native_rle.c',build.TARGET/'src/native_audio_bank.c',build.TARGET/'src/native_assets.c',build.TARGET/'src/native_muted_audio.c',build.TARGET/'src/native_audio_pcm.c',build.TARGET/'src/native_dsp_mix.c',
                   build.TARGET/'src/native_game_video.cpp',*[build.source_path(Path(name)) for name in ('src/boss.c','src/snd.c','src/audi.c','src/memp.c','src/game/indy_comms.c','src/music.c','src/game/lv.c','src/game/title.c','src/game/title2.c','src/game/front.c','src/game/image_bank.c','src/game/prop.c','src/game/loadobjectmodel.c')],*[build.UP/'port/src'/name for name in
                   ('gimgfixup.c','pc_netorder.c','eepimport.c','legacycrc.c','d318watchdog.c','ucode.c')]]:
        cpp=source.suffix=='.cpp';flags=build_probe.engine_args()
        if cpp:flags=[f for f in flags if f not in ('-std=gnu11','-fplan9-extensions','-fpermissive')]+['-std=gnu++20','-fno-rtti','-fno-exceptions','-I'+str(build.UP/'port/fast3d')]
        obj=out/(source.name+'.o')
        original_unit=source.name
        compiler=build.CC.with_name('arm-none-eabi-g++') if cpp else build.CC
        if not cpp:
            import rom_data_plan
            source=rom_data_plan.rewrite(source.name,source,[compiler,*flags])
        build_probe.run([compiler,*flags,'-save-temps=obj','-c',source,'-o',obj])
        if not cpp:
            import original_string_plan
            original_string_plan.patch_object(original_unit,obj)
        objects.append(obj)
    arch=['-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft']
    platform=out/'platform.o'
    build_probe.run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__',
         '-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-I/opt/devkitpro/libctru/include',
         '-c',build.TARGET/'src/native_game_platform.c','-o',platform])
    import opendsp_deps
    opendsp_library=opendsp_deps.library()
    audio=out/'audio_3ds.o'
    build_probe.run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__',
         '-I'+str(build.TARGET/'include'),'-I'+str(opendsp_deps.include()),'-I/opt/devkitpro/libctru/include',
         '-c',build.TARGET/'src/audio_3ds.c','-o',audio])
    # These support objects used to come from the last diagnostic build, which
    # could silently leave gameplay linked against stale implementations.
    for name in ('native_ram.c','native_cart.c','game_memory.c','native_convert.c','native_rom_data.c'):
        obj=out/(name+'.o')
        build_probe.run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__',
                         '-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-Wno-misleading-indentation','-c',build.TARGET/'src'/name,'-o',obj])
        objects.append(obj)
    derived=out/'native_derived.cpp.o'
    build_probe.run([build.CC.with_name('arm-none-eabi-g++'),*arch,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fno-rtti','-fno-exceptions','-ffunction-sections','-fdata-sections','-D__3DS__',
        '-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-c',build.TARGET/'src/native_derived.cpp','-o',derived])
    objects.append(derived)
    bottom=out/'bottom_3ds.o'
    build_probe.run([build.CC,*arch,'-std=gnu11','-O2','-Wall','-Wextra','-Werror','-D__3DS__',
        '-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-I/opt/devkitpro/libctru/include','-c',build.TARGET/'src/bottom_3ds.c','-o',bottom])
    import updater_deps
    objects += updater_deps.objects()
    objects += [audio,platform,bottom,*[p for p in build_graphics.graphics_objects() if p.name in ('gfx_native.cpp.o','gfx_pica.cpp.o','display_3ds.c.o','pica_inputs.cpp.o')]]
    objects += [build.BUILD/name for name in ('input_3ds.o','rom_fixups.o','romassets.o','runtime_symbols.o')]
    target=build.BUILD/'eyes-of-gold-native-game'
    build_probe.run([build.CC.with_name('arm-none-eabi-g++'),*arch,'-specs=3dsx.specs','-L/opt/devkitpro/libctru/lib',
       '-Wl,--gc-sections,--no-enum-size-warning,-Map,'+str(target.with_suffix('.map')),*objects,
       '-Wl,--start-group',*[build.BUILD/name for name in ('liboriginal-engine.a','liboriginal-rsp.a','libnative-os.a','libnative-mixer.a','libnative-baseline.a')],
       '-Wl,--end-group',*updater_deps.libraries(),opendsp_library,'-lcitro3d','-lctru','-lm','-o',target.with_suffix('.elf')])
    symbols=build_probe.run([build.CC.with_name('arm-none-eabi-nm'),'--defined-only',target.with_suffix('.elf')]).stdout
    names={line.split()[-1] for line in symbols.splitlines() if line.split()}
    required={'init_menu00_legalscreen','init_menu04_goldeneyelogo','init_menu05_fileselect','constructor_menu0A_briefing','frontChangeMenu','bossEntry','bossMainloop','lvlStageLoad','gunTickGameplay','chrlvTickPatrol','chrlvTickAttackCommon','bondviewPlayerTickDamageAndHealth','handles_shot_actors','bheadUpdate'}
    assert required<=names,required-names
    assert not {'native_world_render','native_motion_tick','engine_probe'}&names
    (out/'linked-systems.json').write_text(json.dumps({'original_systems':sorted(required),'hardware_verified':False,'audio_enabled':True},indent=2)+'\n')
    print('Original boss/game loop linked:',target.with_suffix('.elf'))
    import audit_original_data
    audit_original_data.require_clean_release(target.with_suffix('.elf'))
    import re
    version_header=(build.TARGET/'include/native_version.h').read_text()
    metadata={key:re.search(r'^#define '+key+r' "([^"]+)"',version_header,re.M).group(1)
              for key in ('NATIVE_LAUNCHER_TITLE','NATIVE_LAUNCHER_DESCRIPTION')}
    build_probe.run(['/opt/devkitpro/tools/bin/smdhtool','--create',metadata['NATIVE_LAUNCHER_TITLE'],metadata['NATIVE_LAUNCHER_DESCRIPTION'],'Eyes of Gold',build.ROOT/'launcher-icon.png',target.with_suffix('.smdh')])
    package=target.with_suffix('.package.elf')
    tokens=prepare_package_elf(target.with_suffix('.elf'),package)
    print('Fixed N64 tokens preserved:',len(tokens))
    build_probe.run(['/opt/devkitpro/tools/bin/3dsxtool',package,target.with_suffix('.3dsx'),'--smdh='+str(target.with_suffix('.smdh'))])
    print('Validated game 3DSX relocations:',validate_3dsx(target.with_suffix('.3dsx')))
    sd=build.ROOT/'local/native-sd/3ds/eyes-of-gold-native-game';sd.mkdir(parents=True,exist_ok=True)
    files=[target.with_suffix('.3dsx'),target.with_suffix('.smdh')]
    packaged=[]
    for source in files:
        name='eyes-of-gold-launcher'+source.suffix
        shutil.copy2(source,sd/name);packaged.append(name)
    # Retire generated copies with the old name to avoid duplicate launcher entries.
    for source in files:
        (sd/source.name).unlink(missing_ok=True)
    notices=['GAME-README.txt','LICENSE.txt','NOTICE.txt','CREDITS.txt']
    for name in notices:
        shutil.copyfile(build.TARGET/'distribution'/name,sd/name)
    for name in ('LICENSE','NOTICE'):
        filename='upstream-'+name+'.txt'
        shutil.copyfile(build.UP/name,sd/filename)
        notices.append(filename)
    shutil.copyfile(build.UP/'port/fast3d/LICENSE.txt',sd/'fast3d-LICENSE.txt')
    notices.append('fast3d-LICENSE.txt')
    for source in sorted((build.TARGET/'distribution/licenses').glob('*.txt')):
        name='licenses/'+source.name
        (sd/'licenses').mkdir(exist_ok=True)
        shutil.copyfile(source,sd/name);notices.append(name)
    archive=build.ROOT/'local/releases/eyes-of-gold-launcher.zip' 
    archive.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for name in packaged+notices:
            z.write(sd/name,'3ds/eyes-of-gold-native-game/'+name)
    print('Original-game package:',archive)

if __name__=='__main__':main()
