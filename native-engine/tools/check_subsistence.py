"""Exercise the actual intro item/ammo branches and custom cheat helpers."""
from pathlib import Path
import os,struct,zlib
import build,build_probe,stage_assets
from check_guard_pose import function
from native_fixture import canonical_rom

def main():
    out=build.BUILD/'subsistence';out.mkdir(exist_ok=True)
    source=build.source_path(Path('src/game/bondview_r.c')).read_text()
    a=source.index('                case INTROTYPE_ITEM:');b=source.index('                case INTROTYPE_SWIRL:',a)
    cases=source[a:b]
    a=source.index('    bondinvAddInvItem(ITEM_FIST);');b=source.index('    g_CurrentPlayer->field_78',a)
    finish=source[a:b]
    front=build.source_path(Path('src/game/front.c')).read_text()
    names=['static int native_cheat_active(int id)','static u8 *native_cheat_label(int id)',
           'static u8 *native_cheat_value(int id,u8 *value)','static int native_cheat_changes_loadout(int id)','void update_menu15_cheat(void)',
           'static void native_custom_cheats_sync(void)','static void native_cheat_toggle(int id)',
           'void init_menu15_cheat(void)']
    helpers='\n'.join(function(front,n+(' {' if n.startswith('static ') else '\n{')) for n in names)
    assert 'native_cheat_active(arrayUnlockedCheats[var_fp])' in front
    assert 'native_cheat_active(arrayUnlockedCheats[var_fp + native_cheat_split(totalunlockedcheats)])' in front
    assert 'g_CheatActivated[arrayUnlockedCheats' not in front
    # Read all 20 retail loadouts from the user's ROM. Generated test data stays
    # inside the ignored build directory, never in source or the install ZIP.
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');table=stage_assets.files()
    stages=['DAM','FACILITY','RUNWAY','SURFACE','BUNKER1','SILO','FRIGATE','SURFACE2','BUNKER2','STATUE','ARCHIVES','STREETS','DEPOT','TRAIN','JUNGLE','CONTROL','CAVERNS','CRADLE','AZTEC','EGYPT']
    fixtures=[]
    sizes={0:12,1:16,2:16,3:32,4:8,5:8,6:40,7:12,8:8,9:4}
    for short,stage in zip(stage_assets.STAGES,stages):
        address,size=table['Usetup'+short+'Z'];raw=zlib.decompress(rom[address+2:address+size],-15)
        at=struct.unpack_from('>I',raw,8)[0];words=[]
        while at:
            kind=struct.unpack_from('>I',raw,at)[0]
            if kind==9:break
            if kind in (1,2):words.extend(struct.unpack_from('>4i',raw,at))
            at+=sizes[kind]
        words.append(9)
        fixtures.append((stage,words))
    fixture_text='\n'.join('static s32 loadout_'+stage+'[]={'+','.join(map(str,words))+'};' for stage,words in fixtures)
    fixture_text+='\nstatic struct {int stage;s32 *records;} retail[]={'+','.join('{LEVELID_'+stage+',loadout_'+stage+'}' for stage,_ in fixtures)+'};\n'
    fixture=(build.TARGET/'tests/subsistence_host.c').read_text()
    fixture=fixture.replace('/* INTRO_CASES */',cases).replace('/* INTRO_FINISH */',finish).replace('/* CHEAT_HELPERS */',helpers).replace('/* RETAIL_FIXTURES */',fixture_text)
    c=out/'check.c';c.write_text(fixture);binary=out/'check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie',c,build.TARGET/'src/native_settings.c',build.UP/'src/str.c','-o',binary])
    print(build_probe.run([binary],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout,end='')
if __name__=='__main__':main()
