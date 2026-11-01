from pathlib import Path
import build,build_probe

def main():
    build.source_path(Path('src/game/front.c'))
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    obj=out/'frontend-end.o';binary=out/'frontend-end-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.TARGET/'tests/frontend_end_host.c','-o',obj])
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',obj,'-o',binary])
    print(build_probe.run([binary]).stdout)
if __name__=='__main__':
    build.prepare();build_probe.host_runner();main()
