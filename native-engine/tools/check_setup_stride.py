#!/usr/bin/env python3
"""Check actual ARM sizepropdef results against every supported retail setup."""
import csv
import struct
import sys
import zlib
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build
import build_probe
from native_fixture import canonical_rom

def main():
    sys.path.insert(0,str(build.UP/'tools_pc'))
    from d88_propdefs import PROPDEF_N64_WORDS
    from native_game_assets import convert_setup
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    import stage_assets
    setups=[];kinds=set()
    setup_names=['Usetup'+s+'Z' for s in stage_assets.STAGES]+['Ump_setup'+s+'Z' for s in stage_assets.MULTIPLAYER_STAGES]
    for stage in setup_names:
        row=next(r for r in csv.reader((build.UP/'scripts/filelist.u.csv').open()) if Path(r[2]).stem==stage)
        address,size=map(int,row[:2])
        retail=zlib.decompress(rom[address+2:address+size],-15)
        native,_=convert_setup(retail)
        start=struct.unpack_from('>I',retail,12)[0]
        records=[];offset=start
        while True:
            kind=retail[offset+3];words=PROPDEF_N64_WORDS[kind]
            records.append((offset,kind,words));kinds.add(kind)
            # Validate newly exercised mixed-size fields independently of the
            # converter's own inverse walk; pointers/AI bytecode stay in place.
            if kind in (4,6,7,12,13,20,36,40,43,45):
                assert native[offset+4:offset+8]==retail[offset+4:offset+6][::-1]+retail[offset+6:offset+8][::-1]
                assert native[offset+120:offset+128]==retail[offset+120:offset+128]
            if kind in (2,14,30,33,34):
                for off in range(offset+4,offset+4*words,4):
                    assert native[off:off+4]==retail[off:off+4][::-1]
            if kind==11:
                images=offset+4*(32+29*4)
                assert native[images:images+4]==retail[images:images+4]
            if kind==6:
                for off in (offset+128,offset+204,offset+208,offset+220,offset+232):
                    assert native[off:off+4]==retail[off:off+4][::-1]
            if kind==44:
                for off in range(offset+4,offset+20,4):
                    assert native[off:off+4]==retail[off:off+4][::-1]
            if kind==20:
                for off in range(offset+128,offset+180,2):
                    assert native[off:off+2]==retail[off:off+2][::-1]
            if kind==40:
                for off in (offset+132,offset+134):
                    assert native[off:off+2]==retail[off:off+2][::-1]
                assert native[offset+128:offset+132]==retail[offset+128:offset+132][::-1]
            if kind==18:
                assert native[offset+8:offset+12]==retail[offset+8:offset+10][::-1]+retail[offset+10:offset+12]
            offset+=4*words
            if kind==48:break
        assert offset==struct.unpack_from('>I',retail,8)[0]
        # Local AI lists are big-endian bytecode even in the native setup.
        ai=struct.unpack_from('>I',retail,20)[0]
        if ai:
            pos=ai
            while struct.unpack_from('>I',retail,pos)[0]:
                script=struct.unpack_from('>I',retail,pos)[0]
                # The scripts precede the tables in both supported files.
                assert native[script:script+2]==retail[script:script+2]
                pos+=8
        setups.append((stage,retail,native,start,records))
    kinds=sorted(kinds)
    # Inline the real function only for compile-time evaluation. Its native
    # sizeof expressions are resolved by the actual ARM compiler, not host ABI.
    original=build.source_path(Path('src/game/loadobjectmodel.c')).read_text()
    original=original.replace('s32 sizepropdef(PropDefHeaderRecord *pdef)\n{',
        '__attribute__((always_inline)) inline s32 sizepropdef(PropDefHeaderRecord *pdef)\n{',1)
    generated=build.BUILD/'game-assets-host/setup-stride-arm.c'
    generated.write_text(original+'\n'+''.join(
        f'u32 eog_stride_{kind}(void) {{ PropDefHeaderRecord p={{0}}; p.type={kind}; return sizepropdef(&p); }}\n'
        for kind in kinds))
    obj=generated.with_suffix('.o')
    build_probe.run([build.CC,*build_probe.engine_args(),'-O2','-c',generated,'-o',obj])
    strides={}
    with obj.open('rb') as stream:
        elf=ELFFile(stream)
        symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
        for kind in kinds:
            sym=symbols[f'eog_stride_{kind}']
            section=elf.get_section(sym['st_shndx'])
            code=section.data()[sym['st_value']:sym['st_value']+sym['st_size']]
            # Each constant wrapper must compile to MOV r0,#words; BX lr.
            mov,ret=struct.unpack_from('<II',code)
            assert mov&0xffffff00==0xe3a00000 and ret==0xe12fff1e,(kind,code.hex())
            strides[kind]=mov&255
    script_binary=build.BUILD/'game-assets-host/stage-scripts-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-no-pie','-Wl,--gc-sections',
        build.TARGET/'tests/stage_scripts_host.c',build.BUILD/'host/src__game__chrai.c.o','-o',script_binary])
    for stage,retail,native,start,records in setups:
        raw_path=build.BUILD/'game-assets-host'/('setup-'+stage+'-retail.bin')
        native_path=build.BUILD/'game-assets-host'/('setup-'+stage+'-native.bin')
        raw_path.write_bytes(retail);native_path.write_bytes(native)
        print(build_probe.run([script_binary,raw_path,native_path,stage]).stdout)
        offset=start
        for expected,kind,words in records:
            assert offset==expected and native[offset+3]==kind,(stage,offset,expected,kind)
            assert strides[kind]==words,(stage,kind,strides[kind],words)
            offset+=4*strides[kind]
        assert offset==struct.unpack_from('<I',native,8)[0]
        print(f'PASS actual ARM sizepropdef walks all {len(records)} {stage} records to the exact intro boundary, including mixed byte/halfword fields')
    from check_guard_pose import function
    import os
    loader=build.source_path(Path('src/game/loadobjectmodel.c')).read_text()
    # Use the independently evaluated ARM strides; host sizeof(pointer) differs.
    reuse_functions='int sizepropdef(PropDefHeaderRecord *p){switch(p->type){'+''.join(f'case {kind}:return {words};' for kind,words in strides.items())+'default:abort();}}\n'+function(loader,'ObjectRecord *setupFindObjForReuse(')
    reuse_functions+='\n'+function(loader,'ObjectRecord *setupGetPtrToCommandByIndex(')
    (build.BUILD/'game-assets-host/setup-reuse-functions.h').write_text(reuse_functions+'\n')
    reuse_binary=build.BUILD/'game-assets-host/setup-reuse-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(build.BUILD/'game-assets-host'),
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fpack-struct=4','-g','-no-pie','-Wl,--gc-sections',
        build.TARGET/'tests/setup_reuse_host.c','-o',reuse_binary])
    for stage,*_ in setups:
        print(build_probe.run([reuse_binary,build.BUILD/'game-assets-host'/('setup-'+stage+'-native.bin')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)
    functions=function(build.source_path(Path('src/game/loadobjectmodel.c')).read_text(),'s32 sizepropdef(PropDefHeaderRecord *pdef)')
    functions+='\n'+function((build.UP/'src/game/objective_status.c').read_text(),'OBJECTIVESTATUS get_status_of_objective(s32 objectiveNum)')
    (build.BUILD/'game-assets-host/bunker-objective-functions.h').write_text(functions+'\n')
    objective_binary=build.BUILD/'game-assets-host/bunker-objectives-check'
    # Objective fixtures use ARM's four-byte record alignment. Host pointer
    # members are unused by this scalar evaluator but otherwise impose 8-byte alignment.
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(build.BUILD/'game-assets-host'),
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fpack-struct=4','-g','-no-pie','-Wl,--gc-sections',
        build.TARGET/'tests/bunker_objectives_host.c','-o',objective_binary])
    import os
    print(build_probe.run([objective_binary,build.BUILD/'game-assets-host/setup-UsetupsevbunkerZ-native.bin'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)
    # Lists/pads use host-width pointers; only objective scalar records retain
    # native packing. ARM assertions separately verify the deployed layout.
    objective_source=(build.UP/'src/game/objective_status.c').read_text()
    silo_functions=reuse_functions[:reuse_functions.index('ObjectRecord *setupFindObjForReuse(')]
    silo_functions+='\n'+function((build.UP/'src/game/objective.c').read_text(),'void set_parent_cur_obj_deposited_in_room(')
    silo_functions+='\n'+function(objective_source,'void objectivestatusCheckDeposit(')
    silo_functions+='\n'+function(objective_source,'OBJECTIVESTATUS get_status_of_objective(s32 objectiveNum)')
    (build.BUILD/'game-assets-host/silo-objective-functions.h').write_text(silo_functions+'\n')
    silo_binary=build.BUILD/'game-assets-host/silo-objectives-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(build.BUILD/'game-assets-host'),
        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-fpack-struct=4','-g','-no-pie','-Wl,--gc-sections',
        build.TARGET/'tests/silo_objectives_host.c','-o',silo_binary])
    print(build_probe.run([silo_binary,build.BUILD/'game-assets-host/setup-UsetupsiloZ-native.bin'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)



if __name__=='__main__':main()
