#!/usr/bin/env python3
"""Check actual music loaders against independent zlib-decoded retail tracks."""
from pathlib import Path
import struct,zlib,subprocess
import build,build_probe

def main():
    out=build.BUILD/'game-assets-host';out.mkdir(exist_ok=True)
    cart=build.BUILD/'native-cart.bin';rom=cart.read_bytes();base=0x419790
    n=struct.unpack_from('>H',rom,base)[0];assert n==63
    expected=bytearray()
    for i in range(n):
        offset,size,compressed=struct.unpack_from('>IHH',rom,base+4+8*i)
        data=zlib.decompress(rom[base+offset+2:base+offset+compressed],-15)
        assert len(data)==size
        expected+=data
    fixture=out/'music-reference.bin';fixture.write_bytes(expected)
    objects=[]
    for rel in ('src/music.c','src/game/decompress.c','src/game/zlib.c'):
        source=build.source_path(Path(rel));obj=out/(source.name+'.music.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-c',source,'-o',obj]);objects.append(obj)
    for source in (build.TARGET/'src/rom_fixups.c',build.TARGET/'tests/music_load_host.c'):
        obj=out/(source.name+'.music.o')
        build_probe.run(['gcc',*build_probe.engine_args(True),'-c',source,'-o',obj]);objects.append(obj)
    binary=out/'music-load-check'
    build_probe.run(['gcc','-no-pie','-Wl,--gc-sections',*objects,'-o',binary])
    print(build_probe.run([binary,cart,fixture]).stdout)
    failed=subprocess.run([str(binary),str(cart),str(fixture),'--stuck'],capture_output=True,text=True,timeout=5)
    assert failed.returncode and 'stop was not acknowledged by audio thread' in failed.stderr,failed.stderr
    print('PASS unresponsive audio stop reaches an explicit timeout')
if __name__=='__main__':
    build.prepare();build_probe.host_runner();main()
