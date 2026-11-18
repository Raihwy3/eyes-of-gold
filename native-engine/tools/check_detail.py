"""Run original LOD selection and render-only prop fading with shared settings."""
import build,build_probe
from pathlib import Path

def main():
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    print(build_probe.run(['python3',build.TARGET/'tests/prop_proxy_host.py']).stdout)
    packet_binary=out/'room-packets-check'
    flags=['-O2','-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include')]
    build_probe.run(['g++',*flags,'-std=gnu++20','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',build.TARGET/'tests/room_packets_host.cpp','-o',packet_binary])
    import os
    env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
    print(build_probe.run([packet_binary],env=env).stdout)
    model=build.source_path(Path('src/game/model.c')).read_text()
    a=model.index('void modelUpdateDistanceRelations(');b=model.index('void modelApplyDistanceRelations(',a)
    (out/'native-detail-engine.h').write_text(model[a:b])
    prop=build.source_path(Path('src/game/propobj.c')).read_text()
    a=prop.index('f32 chrobjFogVisRangeRelated(');b=prop.index('bool sub_GAME_7F054C58(',a)
    (out/'native-detail-fade.h').write_text(prop[a:b])
    objects=[]
    for source in (build.TARGET/'tests/native_detail_host.c',build.TARGET/'src/native_settings.c',build.TARGET/'src/baseline_options.c'):
        obj=out/(source.name+'.detail.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-c',source,'-o',obj]);objects.append(obj)
    binary=out/'native-detail-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*objects,'-lm','-o',binary])
    print(build_probe.run([binary]).stdout)
    actor=build.source_path(Path('src/game/chr.c')).read_text()
    a=actor.index('            int native_actor_faded=')
    b=actor.index('\n\n            gdl = mrData.gdl;',a)
    (out/'native-actor-lod-body.h').write_text(actor[a:b])
    actor_binary=out/'native-actor-lod-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',build.TARGET/'tests/native_actor_lod_host.c','-lm','-o',actor_binary])
    import os
    env=dict(os.environ);env['ASAN_OPTIONS']='detect_leaks=0'
    print(build_probe.run([actor_binary],env=env).stdout)
    a=prop.index('        int native_pane=')
    b=prop.index('native_object_lod_tier=native_saved_tier;',a)+len('native_object_lod_tier=native_saved_tier;')
    (out/'native-prop-lod-body.h').write_text(prop[a:b])
    prop_binary=out/'native-prop-lod-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',build.TARGET/'tests/native_prop_lod_host.c','-lm','-o',prop_binary])
    print(build_probe.run([prop_binary],env=env).stdout)
    bg=build.source_path(Path('src/game/bg.c')).read_text()
    a=bg.index('Gfx *bgRenderRoomPrimary(Gfx *gdl, s32 room_index)\n{')
    b=bg.index('Gfx *bgRenderRoomSecondary(Gfx *gdl, s32 room_index)\n{',a)
    (out/'native-room-render-body.h').write_text(bg[a:b])
    room_binary=out/'native-room-render-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',build.TARGET/'tests/native_room_render_host.c','-lm','-o',room_binary])
    print(build_probe.run([room_binary],env=env).stdout)
if __name__=='__main__':
    build.prepare();build_probe.host_runner();main()
