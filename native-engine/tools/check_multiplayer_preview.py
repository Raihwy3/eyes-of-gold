"""Check actual frontend policy and the presentation's two-player bound."""
from pathlib import Path
import build,build_probe

def main():
    out=build.BUILD/'multiplayer-preview';out.mkdir(exist_ok=True)
    build.source_path(Path('src/game/front.c'))
    obj=out/'front-check.o';settings_obj=out/'native_settings.o';str_obj=out/'str.o';binary=out/'front-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.TARGET/'tests/multiplayer_preview_host.c','-o',obj])
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.TARGET/'src/native_settings.c','-o',settings_obj])
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.UP/'src/str.c','-o',str_obj])
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',obj,settings_obj,str_obj,'-o',binary])
    print(build_probe.run([binary]).stdout)
    bot_obj=out/'mp-bot-host.o';mp_bot_obj=out/'native_mp_bot.o';bot_bin=out/'mp-bot-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.TARGET/'tests/mp_bot_host.c','-o',bot_obj])
    build_probe.run(['gcc',*build_probe.engine_args(True),'-c',build.TARGET/'src/native_mp_bot.c','-o',mp_bot_obj])
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',bot_obj,mp_bot_obj,settings_obj,str_obj,'-lm','-o',bot_bin])
    print(build_probe.run([bot_bin]).stdout)
    source=(build.TARGET/'src/native_game_video.cpp').read_text()
    assert 'if (getPlayerCount() > 4)' in source
    source=(build.TARGET/'src/input_3ds.c').read_text()
    assert 'int inputConnectedMask(void){return 1;}' in source
    assert 'unsigned state=index==0?(pad.buttons|pressed):0;' in source
    lv_source=build.source_path(Path('src/game/lv.c')).read_text()
    assert 'DL = viSetupCurrentPlayerView(DL);\n                if (get_debug_render_raster() == DEB_BOND_VIEW)\n                {\n                    DL = bondviewRenderDebugBondView(DL);\n                }\n                bgRoomVisibilityRelated();' in lv_source
    print('PASS local preview boundary: physical pad mask remains player 1 only, neutral other pads, original two-player render path allowed')
if __name__=='__main__':build.prepare();main()
