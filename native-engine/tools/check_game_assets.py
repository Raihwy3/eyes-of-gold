#!/usr/bin/env python3
"""Check real original resource-table patching and the ARM32 data layout."""
import build,build_probe
from pathlib import Path
import tempfile
import re

def main():
    build.prepare();build_probe.host_runner()
    import stage_assets
    # Use the engine's independent mission and level tables, rather than
    # repeating converter aliases in the expected result.
    front=(build.UP/'src/game/front.c').read_text()
    bg=(build.UP/'src/game/bg.c').read_text()
    missions=dict((stage,level) for level,stage in re.findall(
        r'(LEVELID_\w+)[^\n]*"Ubrief(\w+)Z"',front))
    levels={level:(background,stan) for level,background,stan in re.findall(
        r'\{(LEVELID_\w+),\s*"([^"]+)",\s*"([^"]+)"',bg)}
    assert set(stage_assets.STAGES)==set(missions), "All twenty original missions must be converted"
    for stage in stage_assets.STAGES:
        background=stage_assets.background_name(stage)
        assert levels[missions[stage]] == (
            'bg/bg_'+background+'_all_p.seg','Tbg_'+background+'_all_p_stanZ'), stage
    print('PASS all supported stage BG/STAN aliases match original mission and level tables')
    # Read only the original active arena rows, excluding commented experiments.
    table=front[front.index('struct mp_stage_setup multi_stage_setups[]'):front.index('  //{getStringID',front.index('struct mp_stage_setup multi_stage_setups[]'))]
    mp_entries=re.findall(r'IMG_MP_\w+,\s*(LEVELID_\w+),',table)
    mp_entries=[level for level in mp_entries if level!='LEVELID_NONE']
    assert len(mp_entries)==len(stage_assets.MULTIPLAYER_STAGES)==11
    setup_table=(build.UP/'src/game/chraidata.c').read_text().split('char *setup_text_pointers[] = {',1)[1].split('};',1)[0]
    setups=re.findall(r'NULL|"([^"\n]+)"',setup_table)
    constants=(build.UP/'src/bondconstants.h').read_text().split('typedef enum LEVELID',1)[1].split('}',1)[0]
    constants=re.sub(r'//[^\n]*','',constants)
    ids={};value=-1
    for name,assigned in re.findall(r'(LEVELID_[A-Z0-9_]+)(?:\s*=\s*(-?\d+))?\s*,',constants):
        value=int(assigned) if assigned else value+1;ids[name]=value
    expected=set()
    for level in mp_entries:
        setup=setups[ids[level]];assert setup.startswith('Usetup') and setup.endswith('Z')
        stage=setup[len('Usetup'):-1];expected.add(stage)
        background=stage_assets.background_name(stage)
        assert levels[level]==('bg/bg_'+background+'_all_p.seg','Tbg_'+background+'_all_p_stanZ')
        assert 'Ump_setup'+stage+'Z' in stage_assets.files()
    assert expected==set(stage_assets.MULTIPLAYER_STAGES)
    print('PASS eleven retail multiplayer arenas have converted BG/STAN resources and multiplayer setups')

    names=[name for name,_,_ in stage_assets.conversions()]
    assert len(names)==len(set(names)), 'Duplicate original resource rows'
    assert names.count('bg_sevx_all_p')==names.count('Tbg_sevx_all_p_stanZ')==1
    assert names.count('UsetupsevxZ')==names.count('UsetupsevxbZ')==1
    print('PASS Surface missions share one BG/STAN conversion with separate setups; no duplicate resource rows')

    import check_unlock_all
    check_unlock_all.main()
    import check_bg_special_portals
    check_bg_special_portals.main()
    import check_detail
    check_detail.main()
    import check_watch_skins
    check_watch_skins.main()
    import check_intro_timing
    check_intro_timing.main()
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    log_binary=out/'native-log-check'
    build_probe.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),build.TARGET/'tests/native_log_buffer_host.c','-pthread','-o',log_binary])
    print(build_probe.run([log_binary]).stdout)
    platform=(build.TARGET/'src/native_game_platform.c').read_text()
    a=platform.index('static void log_args(');b=platform.index('u64 sysGetMicroseconds(',a)
    (out/'native-log-functions.h').write_text(platform[a:b])
    policy_binary=out/'native-log-policy-check'
    build_probe.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-I'+str(out),'-I'+str(build.TARGET/'include'),build.TARGET/'tests/native_log_policy_host.c','-pthread','-o',policy_binary])
    print(build_probe.run([policy_binary]).stdout)
    preferences_binary=out/'native-preferences-check'
    build_probe.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),build.TARGET/'src/native_settings.c',build.TARGET/'tests/native_preferences_host.c','-o',preferences_binary])
    print(build_probe.run([preferences_binary]).stdout)
    fat_binary=out/'native-preferences-fat-check'
    build_probe.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-DNATIVE_TEST_FAT_RENAME','-Drename=native_test_rename','-I'+str(build.TARGET/'include'),build.TARGET/'src/native_settings.c',build.TARGET/'tests/native_preferences_host.c','-o',fat_binary])
    print(build_probe.run([fat_binary]).stdout)
    build.source_path(Path('src/game/options.c'))
    text_source=(build.UP/'src/game/textrelated.c').read_text()
    a=text_source.index('void textMeasure(');b=text_source.index('\nvoid textWrap(',a)
    (out/'native-objective-measure.h').write_text(text_source[a:b])
    menu_objects=[]
    for source in (build.TARGET/'tests/native_menu_host.c',build.TARGET/'src/native_settings.c',build.UP/'src/str.c'):
        obj=out/(source.name+'.menu.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);menu_objects.append(obj)
    menu_binary=out/'native-menu-options-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*menu_objects,'-lm','-o',menu_binary])
    print(build_probe.run([menu_binary]).stdout)
    hid_binary=out/'native-hid-check'
    build_probe.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'tests/input/mock'),'-I'+str(build.TARGET/'include'),build.TARGET/'src/input_3ds.c',build.TARGET/'src/native_settings.c',build.TARGET/'tests/input/check.c','-lm','-o',hid_binary])
    print(build_probe.run([hid_binary]).stdout)
    import check_bottom
    check_bottom.main()
    platform=(build.TARGET/'src/native_game_platform.c').read_text()
    assert 'svcExitProcess(' not in platform and 'native_exit_deadline' not in platform
    a=platform.index('  /* Use the normal SDK APT/loader return.')
    shutdown=platform[a:platform.rfind('}')]
    assert shutdown.rstrip().endswith('return 0;')
    (out/'native-shutdown-body.h').write_text(shutdown)
    audio=(build.TARGET/'src/audio_3ds.c').read_text()
    a=audio.index('int native_audio_shutdown(void) {');b=audio.index('static uint32_t cursor',a)
    (out/'native-audio-shutdown.h').write_text(audio[a:b])
    shutdown_binary=out/'native-shutdown-check'
    build_probe.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(out),'-I'+str(build.TARGET/'include'),build.TARGET/'tests/native_shutdown_host.c','-o',shutdown_binary])
    print(build_probe.run([shutdown_binary]).stdout)
    a=platform.index('static struct NativeRenderStart {');b=platform.index('void native_platform_worker_info(',a)
    (out/'native-worker-creation.h').write_text(platform[a:b])
    worker_binary=out/'native-worker-check'
    build_probe.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(out),'-I'+str(build.TARGET/'include'),build.TARGET/'tests/native_worker_host.c','-o',worker_binary])
    print(build_probe.run([worker_binary]).stdout)
    pickup=build.source_path(Path('src/game/propobj.c')).read_text()
    a=pickup.index('    if (!native_pickup_look_allowed(')
    b=pickup.index('\n\n\n/*\n* Address: 7F050D30',a)
    (out/'native-pickup-position.h').write_text(pickup[a:b])
    pickup_binary=out/'native-pickup-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-no-pie',build.TARGET/'tests/native_pickup_host.c','-o',pickup_binary])
    print(build_probe.run([pickup_binary]).stdout)
    movement=build.source_path(Path('src/game/bondview2.c')).read_text()
    start=movement.index('        if (moveData.digitalStepLeft)',movement.index('    native_controls_apply(&moveData);'))
    end=movement.index('        if (g_CurrentPlayer->speedforwards > 1.0f)',start)
    (out/'native-movement-commit.h').write_text(movement[start:end])
    assert 'native_controls_adjust_look(&g_CurrentPlayer' not in movement
    assert movement.count('native_controls_look_rate(g_CurrentPlayer->speed')==2
    a=movement.index('f32 sub_GAME_7F080228(');b=movement.index('Gfx *currentPlayerDrawFade(',a)
    (out/'native-aim-acceleration.h').write_text(movement[a:b])
    turn=movement.index('    if (moveData.canNaturalTurn)\n    {\n        ftemp_nostack_sp8C')
    turn_end=movement.index('    else if (moveData.aimTurnLeftSpeed > 0)',turn)
    pitch=movement.index('            if (moveData.canNaturalPitch)\n            {')
    pitch_end=movement.index('            else if (moveData.speedVertaDown > 0)',pitch)
    (out/'native-touch-aim-axes.h').write_text(movement[turn:turn_end]+movement[pitch:pitch_end])
    control_objects=[]
    for source in (build.TARGET/'src/native_controls.c',build.TARGET/'src/native_settings.c',build.UP/'src/str.c',build.TARGET/'tests/native_controls_host.c'):
        obj=out/(source.name+'.controls.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);control_objects.append(obj)
    controls_binary=out/'native-controls-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*control_objects,'-lm','-o',controls_binary])
    print(build_probe.run([controls_binary]).stdout)
    cache_binary=out/'native-cart-cache-check'
    build_probe.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),build.TARGET/'src/native_cart.c',build.TARGET/'tests/native_cart_cache_host.c','-pthread','-o',cache_binary])
    print(build_probe.run([cache_binary]).stdout)
    front=build.source_path(Path('src/game/front.c')).read_text()
    cursor_objects=[]
    for source in (build.TARGET/'tests/native_front_cursor_host.c',build.TARGET/'src/baseline_options.c',build.TARGET/'src/native_settings.c'):
        obj=out/(source.name+'.cursor.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);cursor_objects.append(obj)
    cursor_binary=out/'native-front-cursor-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*cursor_objects,'-lm','-o',cursor_binary])
    print(build_probe.run([cursor_binary]).stdout)
    start=front.index('static f32 native_frontend_aspect(')
    end=front.index('\n}',start)+2
    (out/'native-front-aspect.h').write_text(front[start:end]+'\n')
    aspect_objects=[]
    for source in (build.source_path(Path('src/game/options.c')),build.source_path(Path('src/game/bondview2.c')),build.source_path(Path('src/game/bg.c')),build.TARGET/'src/baseline_options.c',build.TARGET/'src/native_settings.c',build.UP/'src/str.c',build.TARGET/'tests/native_aspect_host.c'):
        obj=out/(source.name+'.aspect.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);aspect_objects.append(obj)
    aspect_binary=out/'native-aspect-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*aspect_objects,'-lm','-o',aspect_binary])
    print(build_probe.run([aspect_binary]).stdout)
    import check_frontend_end
    check_frontend_end.main()
    import check_music_load
    check_music_load.main()
    import check_native_audio
    check_native_audio.main()
    import check_setup_stride
    check_setup_stride.main()
    import check_model_bsp
    check_model_bsp.main()
    boot_objects=[]
    for source in (build.TARGET/'src/native_game.c',build.UP/'src/token.c',build.UP/'src/str.c',build.TARGET/'tests/native_boot_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);boot_objects.append(obj)
    boot_binary=out/'native-boot-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*boot_objects,'-o',boot_binary])
    print(build_probe.run([boot_binary]).stdout)
    planner_binary=out/'planner-equivalence-check'
    build_probe.run(['g++','-std=c++20','-O2','-I'+str(build.TARGET/'include'),build.TARGET/'src/pica_inputs.cpp',build.TARGET/'tests/pica_inputs_reference.cpp',build.TARGET/'tests/pica_inputs_equivalence.cpp','-o',planner_binary])
    print(build_probe.run([planner_binary]).stdout)
    material_binary=out/'pica-material-check'
    build_probe.run(['g++','-std=c++20','-O1','-I'+str(build.TARGET/'include'),'-I'+str(build.UP/'port/fast3d'),build.TARGET/'tests/pica_material_host.cpp',build.TARGET/'src/pica_inputs.cpp',build.UP/'port/fast3d/gfx_cc.cpp','-o',material_binary])
    print(build_probe.run([material_binary]).stdout)
    slot_objects=[]
    for source in (build.source_path(Path('src/game/model.c')),build.source_path(Path('src/game/initunk_005520.c')),build.TARGET/'tests/model_slots_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);slot_objects.append(obj)
    slot_binary=out/'model-slots-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*slot_objects,'-lm','-o',slot_binary])
    print(build_probe.run([slot_binary]).stdout)
    indy_objects=[]
    for source in (build.source_path(Path('src/game/indy_comms.c')),build.UP/'src/game/indy_commands.c',build.UP/'src/str.c',build.TARGET/'tests/indy_disconnected_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);indy_objects.append(obj)
    indy_binary=out/'indy-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*indy_objects,'-o',indy_binary])
    print(build_probe.run([indy_binary]).stdout)
    music_objects=[]
    for source in (build.source_path(Path('src/music.c')),build.source_path(Path('src/snd.c')),build.TARGET/'src/native_muted_audio.c',build.TARGET/'tests/music_muted_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);music_objects.append(obj)
    music_binary=out/'music-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*music_objects,'-o',music_binary])
    print(build_probe.run([music_binary]).stdout)
    bank=build.source_path(Path('src/game/image_bank.c'))
    offsets=re.findall(r'g_pc_gimg_off_(\w+) = (0x[0-9a-fA-F]+)',bank.read_text())
    assert len(offsets)==49
    arm_check=out/'image-bank-arm-layout.c'
    arm_check.write_text('#include "'+str(bank)+'"\n'+''.join('_Static_assert(GIMG_OFF('+name+') == 0x02000000u + '+offset+', "ARM segment offset '+name+'");\n' for name,offset in offsets))
    build_probe.run([build.CC,*build_probe.engine_args(),'-c',arm_check,'-o',out/'image-bank-arm-layout.o'])
    print('PASS all 49 global texture symbols use fixed segment offsets on ARM32')
    image_objects=[]
    for source in (bank,build.UP/'port/src/gimgfixup.c',build.UP/'assets/oddtextures.c',build.TARGET/'tests/global_image_reset_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);image_objects.append(obj)
    image_binary=out/'image-bank-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*image_objects,build.BUILD/'host/romassets.o','-o',image_binary])
    print(build_probe.run([image_binary,build.BUILD/'native-cart.bin']).stdout)
    pool=build.source_path(Path('src/game/objecthandler.c'))
    pool_arm=out/'model-hit-arm-layout.c'
    pool_arm.write_text('#include "'+str(pool)+'"\n_Static_assert(sizeof(g_ModelHitEntries)==600*sizeof(ModelHitEntry), "ARM model-hit pool capacity");\n')
    build_probe.run([build.CC,*build_probe.engine_args(),'-c',pool_arm,'-o',out/'model-hit-arm-layout.o'])
    print('PASS ARM32 model-hit pool storage holds 600 actual records')
    pool_objects=[]
    for source in (pool,build.source_path(Path('src/game/initunk_005450.c')),build.source_path(Path('src/game/bg.c')),build.TARGET/'tests/native_pool_opcode_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);pool_objects.append(obj)
    pool_binary=out/'pool-opcode-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*pool_objects,'-o',pool_binary])
    print(build_probe.run([pool_binary]).stdout)
    objects=[]
    for source in (build.UP/'src/game/ob.c',build.UP/'src/deb.c',build.UP/'src/str.c',build.TARGET/'src/native_assets.c',build.TARGET/'tests/game_assets_host.c'):
        obj=out/(source.name+'.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);objects.append(obj)
    binary=out/'check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections,-Ttext-segment=0x02000000',*objects,build.BUILD/'host/src__memp.c.o',build.BUILD/'host/native_cart.o',build.BUILD/'host/romassets.o','-pthread','-lm','-o',binary])
    result=build_probe.run([binary,build.BUILD]);print(result.stdout)
    build_probe.run([build.CC,*build_probe.engine_args(),'-c',build.TARGET/'tests/native_game_layout.c','-o',out/'native-layout.o'])
    print('PASS original ARM32 stage/model/prop layout contract')
    with tempfile.TemporaryDirectory(prefix='eog-assets-') as temp:
        folder=Path(temp)
        (folder/'native-cart.bin').symlink_to(build.BUILD/'native-cart.bin')
        original=(build.BUILD/'native-game.dat').read_bytes()
        damaged=bytearray(original);damaged[-1]^=1
        (folder/'native-game.dat').write_bytes(damaged)
        print(build_probe.run([binary,folder,'--invalid']).stdout)
        (folder/'native-game.dat').write_bytes(original)
        (folder/'native-cart.bin').unlink();(folder/'native-cart.bin').write_bytes(b'wrong ROM')
        print(build_probe.run([binary,folder,'--invalid']).stdout)

if __name__=='__main__':main()
