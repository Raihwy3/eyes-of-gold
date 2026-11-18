"""Actual watch-skin command generation and original-hand replacement scope."""
import os
from pathlib import Path
import build,build_probe

def main():
    source=build.source_path(Path('src/game/bondview2.c')).read_text()
    a=source.index('Gfx *bondviewRenderWatch(');b=source.index('    return gdl;',a)
    assert 'native_watch_environment_rgba(' in source[a:b] and 'renderdata.envcolour.word = 0xcd;' not in source[a:b]
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    flags=['-fsanitize=address,undefined','-g','-fno-omit-frame-pointer'] if os.environ.get('EOG_SKIN_SANITIZE') else []
    objs=[]
    for source in (build.TARGET/'tests/watch_skin_host.c',build.TARGET/'src/native_settings.c',build.UP/'src/game/matrixmath.c'):
        obj=out/(source.name+'.skin.o');build_probe.run(['gcc',*build_probe.engine_args(True),*flags,'-c',source,'-o',obj]);objs.append(obj)
    binary=out/'native-watch-skin-check';build_probe.run(['gcc',*flags,'-no-pie','-Wl,--gc-sections',*objs,'-lm','-o',binary]);print(build_probe.run([binary]).stdout)
if __name__=='__main__':
    build.prepare();build_probe.host_runner();main()
