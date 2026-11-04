"""Test setup shuffling against all campaign setup records from the user's ROM."""
from pathlib import Path
import os,struct,zlib,sys
import build,build_probe,stage_assets
from native_fixture import canonical_rom
sys.path.insert(0,str(build.UP/'tools_pc'))
from d88_propdefs import PROPDEF_N64_WORDS

def main():
    constructor=build.source_path(Path('src/game/chraction.c')).read_text()
    assert 'native_randomizer_story_appearance(arg1->chrnum,&visualbody,&visualhead)' in constructor
    assert 'temp_v0_5->bodynum = (s8) bodyid;' in constructor
    assert 'temp_v0_5->headnum = (s8) headid;' in constructor
    out=build.BUILD/'randomizer';out.mkdir(exist_ok=True)
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');files=stage_assets.files();functions=[]
    for n,stage in enumerate(stage_assets.STAGES):
        address,size=files['Usetup'+stage+'Z'];raw=zlib.decompress(rom[address+2:address+size],-15)
        at=struct.unpack_from('>I',raw,12)[0];lines=[];index=0
        while at:
            scale,state,kind=struct.unpack_from('>HBB',raw,at)
            if kind==48:break
            lines.append(f'r[{index}].header.type={kind};r[{index}].header.extrascale={scale};r[{index}].header.state={state};')
            fields={}
            if kind==9:
                names=['chrnum','PadID','BodyID','AIListID','Preset','chrpreset1','health','ReactionTime','bitflags','HeadID']
                fields={name:struct.unpack_from('>h' if name=='HeadID' else '>H',raw,at+4+2*i)[0] for i,name in enumerate(names)};member='guard'
            elif kind in (8,20):
                fields={'obj':struct.unpack_from('>H',raw,at+4)[0],'pad':struct.unpack_from('>H',raw,at+6)[0],'flags':struct.unpack_from('>I',raw,at+8)[0],'flags2':struct.unpack_from('>I',raw,at+12)[0]};member='weapon' if kind==8 else 'ammo'
                if kind==8:fields['weaponnum']=struct.unpack_from('>b',raw,at+128)[0]
                else:
                    for k in range(13):
                        model,quantity=struct.unpack_from('>HH',raw,at+128+4*k)
                        lines.append(f'r[{index}].ammo.slots[{k}].modelnum={model};r[{index}].ammo.slots[{k}].quantity={quantity};')
            elif kind==22:fields={'OffsetToObj':struct.unpack_from('>h',raw,at+6)[0]};member='tag'
            elif kind in (14,19):fields={'Index1':struct.unpack_from('>i',raw,at+4)[0],'Index2':struct.unpack_from('>i',raw,at+8)[0]};member='link'
            elif kind==44:fields={'Index1':struct.unpack_from('>i',raw,at+4)[0],'Index2':struct.unpack_from('>i',raw,at+8)[0],'Index3':struct.unpack_from('>i',raw,at+12)[0]};member='safe'
            lines.extend(f'r[{index}].{member}.{key}={value};' for key,value in fields.items())
            at+=PROPDEF_N64_WORDS[kind]*4;index+=1
        assert index<4096
        lines.append(f'r[{index}].header.type=PROPDEF_END;count={index};')
        at=struct.unpack_from('>I',raw,8)[0];words=[];sizes=[3,4,4,8,2,2,10,3,2]
        while at:
            kind=struct.unpack_from('>i',raw,at)[0]
            if kind==9:words.append(9);break
            words.extend(struct.unpack_from('>'+str(sizes[kind])+'i',raw,at));at+=sizes[kind]*4
        lines.append('s32 intro[]={'+','.join(map(str,words))+'};')
        lines.append(f'check_stage({n+1},intro,sizeof intro);')
        functions.append('static void fixture_'+str(n)+'(void){memset(r,0,sizeof r);'+''.join(lines)+'}')
    tables=(build.UP/'src/game/mp_weapon.c').read_text().split('struct s_mp_weapon_set_text mp_weapon_set_text_table')[0]
    fixture=(build.TARGET/'tests/randomizer_host.c').read_text().replace('/* WEAPON_TABLES */',tables).replace('/* FIXTURES */','\n'.join(functions)).replace('/* RUN */',''.join(f'fixture_{n}();' for n in range(20)))
    c=out/'check.c';c.write_text(fixture);binary=out/'check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie',c,build.TARGET/'src/native_settings.c',build.UP/'src/str.c','-o',binary])
    print(build_probe.run([binary],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout,end='')
if __name__=='__main__':main()
