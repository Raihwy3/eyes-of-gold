"""Compare on-console generated geometry/UI data against host-only references."""
import os,struct,subprocess,tempfile
from pathlib import Path
import build,build_probe,derived_layout,bottom_background,title_asset_layout
from native_fixture import canonical_rom,fnv

def compile_objects(sanitize=False,original=False):
    derived_layout.generate();title_asset_layout.generate();out=build.BUILD/'derived-host';out.mkdir(exist_ok=True)
    common=['-O2','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include')]
    if sanitize:common+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    objs=[]
    for name,compiler,flags in [('native_derived.cpp','g++',['-std=c++20']),('native_convert.c','gcc',['-std=c11'])]:
        obj=out/(name+'.o');subprocess.run([compiler,*common,*flags,'-c',build.TARGET/'src'/name,'-o',obj],check=True);objs.append(obj)
    shim=out/'inflate.o';flags=['-DCONVERT_NO_MAIN']+(['-DCONVERT_ORIGINAL_INFLATE'] if original else [])
    subprocess.run(['gcc',*common,'-std=c11',*flags,'-c',build.TARGET/'tests/native_convert_host.c','-o',shim],check=True)
    objs.append(shim)
    storage=out/'title-storage.o'
    subprocess.run(['gcc',*common,'-c',build.TARGET/'tests/native_title_storage_host.c','-o',storage],check=True)
    objs.append(storage)
    if original:objs += [build.BUILD/'host'/name for name in ('src__game__zlib.c.o','src__game__decompress.c.o')]
    return common,objs

def main():
    bottom_background.generate();common,objs=compile_objects(bool(os.getenv('EOG_DERIVED_SANITIZE')),bool(os.getenv('EOG_DERIVED_ORIGINAL')))
    out=build.BUILD/'derived-host';binary=out/'derived';reference=out/'reference'
    io=[build.TARGET/'tests/native_derived_io.cpp','-Wl,--wrap=fwrite,--wrap=fseek,--wrap=fclose']
    subprocess.run(['g++',*common,'-std=c++20','-no-pie',build.TARGET/'tests/native_derived_host.cpp',*io,*objs,'-lz','-o',binary],check=True)
    subprocess.run(['g++',*common,'-std=c++20',build.TARGET/'tests/native_derived_reference.cpp','-o',reference],check=True)
    expected=out/'reference.dat';subprocess.run([reference,expected],check=True)
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');title=bytearray(rom[0x29e560:0x2a4d50])
    for name,offset,size,kind in title_asset_layout.generate():
        if kind==2:
            for at in range(offset,offset+size,16):
                for j in range(0,12,2):title[at+j:at+j+2]=title[at+j:at+j+2][::-1]
        elif kind in (1,3):
            for at in range(offset,offset+size,4):title[at:at+4]=title[at:at+4][::-1]
    # Check native title bytes/command encodings against the previous compiled
    # original arrays, with runtime pointer relocations compared separately.
    old=out/'title-reference.o'
    subprocess.run([build.CC,*build_probe.engine_args(),'-c',build.UP/'assets/rarewarelogo.c','-o',old],check=True)
    for name,offset,size,kind in title_asset_layout.generate():
        section=out/(name+'.bin')
        subprocess.run([build.CC.with_name('arm-none-eabi-objcopy'),'--dump-section','.data.'+name+'='+str(section),old],check=True,capture_output=True)
        compiled=section.read_bytes() if section.exists() else bytes(size)
        actual=bytes(title[offset:offset+size]);assert len(compiled)==len(actual)
        if kind==3:
            for at in range(0,size,8):
                x,y=struct.unpack_from('<2I',actual,at)
                assert actual[at:at+4]==compiled[at:at+4],(name,at,'command encoding')
                if not (x>>24 in (3,4,6,0xfd) and y>>24==2):assert actual[at+4:at+8]==compiled[at+4:at+8],(name,at,'command argument')
        else:assert compiled==actual,(name,'pixels/vertices')
    data=expected.read_bytes();expected.write_bytes(data[:-76800]+title+data[-76800:])
    def parts(data):
        objects,rooms,bg,packets=struct.unpack_from('<4I',data,8);at=32;obj={};rm={}
        for target,count,fmt in [(obj,objects,'<2I'),(rm,rooms,'<3I')]:
            for _ in range(count):
                key=struct.unpack_from(fmt,data,at);at+=struct.calcsize(fmt);n=key[-1];target[key]=data[at:at+n*18];at+=n*18
        at=(at+3)&~3
        # ABI structs are pointer-free with explicit 32-bit fields and arrays.
        bg_size=4+16+4*128*3+128*16;packet_size=12+32*8+128*3
        meshes=data[at:at+bg*bg_size];at+=bg*bg_size
        programs=data[at:at+packets*packet_size];at+=packets*packet_size
        title=data[at:at+0x67f0];at+=0x67f0
        pixels=data[at:];assert len(pixels)==76800
        return obj,rm,meshes,programs,title,pixels
    with tempfile.TemporaryDirectory(dir=out) as temp:
        folder=Path(temp);rom=folder/'rom.z64';rom.write_bytes(canonical_rom(build.ROOT/'local/roms/GoldenEye.n64'))
        target=folder/'native-render.dat'
        subprocess.run([binary,'generate',rom,target],check=True,timeout=180)
        actual=target.read_bytes();(out/'actual.dat').write_bytes(actual);assert fnv(actual[32:])==struct.unpack_from('<I',actual,28)[0]
        # Run the same converter with a live-allocation ceiling. Desktop RAM
        # otherwise hides startup exhaustion on the 24 MiB console heap.
        if not os.getenv('EOG_DERIVED_SANITIZE'):
            budget=out/'derived-budget'
            subprocess.run(['g++',*common,'-std=c++20','-no-pie',build.TARGET/'tests/native_derived_host.cpp',build.TARGET/'tests/native_derived_budget.cpp',*io,*objs,'-lz','-o',budget],check=True)
            limited=folder/'budget.dat'
            subprocess.run([budget,'generate',rom,limited],check=True,timeout=180)
            assert limited.read_bytes()==actual
            print('PASS first-launch rendering conversion: below 16 MiB live C++ allocations, identical cache bytes')
        good_rom=rom.read_bytes()
        for bad_rom in (good_rom[:-1],good_rom+b'\0',good_rom[:4096]+bytes([good_rom[4096]^1])+good_rom[4097:]):
            rom.write_bytes(bad_rom)
            rejected=folder/'rejected.dat'
            assert subprocess.run([binary,'generate',rom,rejected]).returncode==1
            assert not rejected.exists()
        rom.write_bytes(good_rom)
        for failure in ({'EOG_TEST_CACHE_FAIL_AFTER':'0'},
                        {'EOG_TEST_CACHE_FAIL_AFTER':'100003'},
                        {'EOG_TEST_CACHE_FAIL_SEEK':'1'},
                        {'EOG_TEST_CACHE_FAIL_CLOSE':'1'}):
            broken=folder/'failed-output.dat'
            assert subprocess.run([binary,'generate',rom,broken],env={**os.environ,**failure},timeout=180).returncode==1
            assert not broken.exists(),'Failed cache output must be removed'
        print('PASS cache output failures: initial/mid-payload short writes, header seek and close failures remove incomplete output')
        a,b=parts(actual),parts(expected.read_bytes())
        for name,x,y in zip(('objects','rooms','triangle-run LODs','room draw commands','title assets','backdrop pixels'),a,b):
            assert x==y,(name,len(x),len(y),set(x)^set(y) if isinstance(x,dict) else '')
        subprocess.run([binary,'validate',folder],check=True);subprocess.run([binary,'mount',folder],check=True)
        for bad in (b'',actual[:40],actual[:-1],actual[:32]+b'\0'*len(actual[32:])):
            target.write_bytes(bad);assert subprocess.run([binary,'validate',folder]).returncode==1
        # Authenticated payload with corrupt structure: checksum alone is insufficient.
        bad=bytearray(actual);struct.pack_into('<I',bad,36,0xffffffff);struct.pack_into('<I',bad,28,fnv(bad[32:]));target.write_bytes(bad)
        assert subprocess.run([binary,'mount',folder]).returncode==1
    print('PASS console derived assets: exact object/room LODs, triangle-run LODs, room packets and backdrop; cache validation, mount and lifetime')
if __name__=='__main__':main()
