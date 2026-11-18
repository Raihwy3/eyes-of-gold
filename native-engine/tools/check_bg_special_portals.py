"""Run the actual background portal-table walker with a poisoned linker gap."""
import json,os,zlib
from pathlib import Path
import build,build_probe
from check_guard_pose import function
from native_fixture import canonical_rom

def main():
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    row=next(r for r in json.loads((build.TARGET/'rom-data-layout.json').read_text())['rows'] if r['symbol']=='specialportalarray')
    assert row['bytes']==20 and row['bank']=='cdata'
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    bank=zlib.decompress(rom[0x21992:0x331e0],-15)
    fixture=out/'special-portals-retail.bin';fixture.write_bytes(bank[row['source_offset']:row['source_offset']+row['bytes']])
    source=build.source_path(Path('src/game/bg.c')).read_text()
    header=out/'bg-special-portals-function.h'
    header.write_text(function(source,'void sub_GAME_7F0B37EC(void)')+'\n')
    binary=out/'bg-special-portals-check'
    build_probe.run(['gcc',*build_probe.engine_args(True),'-I'+str(out),'-fsanitize=address,undefined','-fno-omit-frame-pointer','-g','-no-pie','-Wl,--gc-sections',build.TARGET/'tests/bg_special_portals_host.c','-o',binary])
    print(build_probe.run([binary,fixture],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}).stdout)

if __name__=='__main__':main()
