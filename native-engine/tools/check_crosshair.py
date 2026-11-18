"""Run the actual generated sight renderer and native persistence hook."""
from pathlib import Path
import build,build_probe

def main():
    build.prepare()
    out=build.BUILD/'crosshair-host';out.mkdir(exist_ok=True)
    source=build.source_path(Path('src/game/gunfire.c')).read_text()
    start=source.index('void gunDrawSight(s32 *gdl) {');end=source.index('\n}',start)+2
    (out/'native-sight-draw.h').write_text(source[start:end])
    source=(build.TARGET/'src/baseline_options.c').read_text()
    start=source.index('int portCrosshairPersistent(void){');end=source.index('\n}',start)+2
    (out/'native-persistent-sight.h').write_text(source[start:end])
    binary=out/'sight-check'
    build_probe.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fno-strict-aliasing','-I'+str(out),'-I'+str(build.TARGET/'include'),build.TARGET/'tests/crosshair_draw_host.c',build.TARGET/'src/native_settings.c','-o',binary])
    print(build_probe.run([binary]).stdout)
    shapes=out/'shapes-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-no-pie','-Wl,--gc-sections',build.TARGET/'tests/crosshair_shapes_host.c',build.TARGET/'src/native_settings.c','-o',shapes])
    print(build_probe.run([shapes]).stdout)
if __name__=='__main__':main()
