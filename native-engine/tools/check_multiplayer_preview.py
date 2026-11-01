"""Check actual frontend policy and the presentation's two-player bound."""
from pathlib import Path
import build,build_probe

def main():
    out=build.BUILD/'multiplayer-preview';out.mkdir(exist_ok=True)
    build.source_path(Path('src/game/front.c'))
    obj=out/'front-check.o';binary=out/'front-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.TARGET/'tests/multiplayer_preview_host.c','-o',obj])
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',obj,'-o',binary])
    print(build_probe.run([binary]).stdout)
    source=(build.TARGET/'src/native_game_video.cpp').read_text()
    assert 'if (getPlayerCount() > 2)' in source
    source=(build.TARGET/'src/input_3ds.c').read_text()
    assert 'int inputConnectedMask(void){return 1;}' in source
    assert 'unsigned state=index==0?(pad.buttons|pressed):0;' in source
    print('PASS local preview boundary: physical pad mask remains player 1 only, neutral other pads, original two-player render path allowed')
if __name__=='__main__':build.prepare();main()
