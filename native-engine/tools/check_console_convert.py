"""Exercise first boot with only a ROM, all byte orders, cache reuse and retry."""
import os,struct,subprocess,tempfile,zlib
from pathlib import Path
import build,console_asset_recipe
from native_fixture import canonical_rom,fnv

def main(original=False):
    console_asset_recipe.generate()
    out=build.BUILD/'console-convert';out.mkdir(exist_ok=True)
    binary=out/'convert-host'
    import check_derived
    common,objects=check_derived.compile_objects(bool(os.getenv('EOG_CONVERT_SANITIZE')),original or bool(os.getenv('EOG_CONVERT_ORIGINAL')))
    main_obj=out/'main.o'
    subprocess.run(['gcc',*common,'-std=c11','-DCONVERT_EXTERNAL_INFLATE','-c',build.TARGET/'tests/native_convert_host.c','-o',main_obj],check=True)
    subprocess.run(['g++',*common,'-no-pie',main_obj,*objects,'-lz','-o',binary],check=True)
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    expected=(build.BUILD/'native-game.dat').read_bytes();pcm=(build.BUILD/'native-audio.pcm').read_bytes()
    def files(data):
        rows={}
        assert data[:8]==b'EOGGAME1'
        assert fnv(data[24:])==struct.unpack_from('<I',data,16)[0]
        for at in range(24,24+struct.unpack_from('<I',data,12)[0]*80,80):
            name,offset,size,raw,h=struct.unpack_from('<64s4I',data,at);name=name.rstrip(b'\0')
            blob=data[offset:offset+size];assert fnv(blob)==h
            rows[name]=zlib.decompress(blob[2:],-15) if blob[:2]==b'\x11\x72' else blob
            assert len(rows[name])==raw
        return rows
    expected_files=files(expected)
    with tempfile.TemporaryDirectory(dir=out) as folder:
        folder=Path(folder)
        for order,name in [(0,'GoldenEye.z64'),(1,'GoldenEye.v64'),(3,'GoldenEye.n64')]:
            image=bytes(rom) if not order else bytes(rom[i^order] for i in range(len(rom)))
            source=folder/name;source.write_bytes(image)
            subprocess.run([binary,folder],check=True,timeout=60)
            assert (folder/'native-cart.bin').read_bytes()==rom
            actual=files((folder/'native-game.dat').read_bytes());assert actual==expected_files
            assert (folder/'native-audio.pcm').read_bytes()==pcm,'PCM differs from desktop predictor-context reference'
            assert (folder/'native-render.dat').is_file()
            source.unlink()
            before={p.name:p.stat().st_mtime_ns for p in folder.iterdir()}
            subprocess.run([binary,folder],check=True,timeout=60) # Works without ROM after completion.
            assert before=={p.name:p.stat().st_mtime_ns for p in folder.iterdir()}
            for p in folder.iterdir():p.unlink()
        source=folder/'GoldenEye.z64';source.write_bytes(rom)
        result=subprocess.run([binary,folder],env={**os.environ,'CONVERT_CANCEL':'1'},capture_output=True,timeout=60);assert result.returncode==1
        assert not list(folder.glob('*.tmp'))
        for stage in ('Checking ROM','Converting models and level','Decoding sound effects','Building object LODs','Building room LODs and draw lists'):
            result=subprocess.run([binary,folder],env={**os.environ,'CONVERT_CANCEL_STAGE':stage},capture_output=True,timeout=60)
            assert result.returncode==1 and b'cancelled' in result.stderr
            assert not list(folder.glob('*.tmp')) and not list(folder.glob('native-*'))
        bad=bytearray(rom);bad[4096]^=1;source.write_bytes(bad)
        result=subprocess.run([binary,folder],capture_output=True,timeout=60);assert result.returncode==1 and b'SHA-1' in result.stderr
        assert not list(folder.glob('*.tmp')) and not list(folder.glob('native-*'))
        source.write_bytes(rom);subprocess.run([binary,folder],check=True,timeout=60)
        legacy=bytearray((folder/'native-game.dat').read_bytes())
        struct.pack_into('<I',legacy,12,len(expected_files)-3)
        (folder/'native-game.dat').write_bytes(legacy)
        legacy_render=bytearray((folder/'native-render.dat').read_bytes());legacy_render[:8]=b'EOGDRW20'
        (folder/'native-render.dat').write_bytes(legacy_render)
        config=folder/'native-options.ini';config.write_text('preserve existing settings')
        subprocess.run([binary,folder],check=True,timeout=180)
        assert files((folder/'native-game.dat').read_bytes())==expected_files
        assert (folder/'native-render.dat').read_bytes()[:8]==b'EOGDRW21'
        assert config.read_text()=='preserve existing settings'
        # Reproduce the faulty Control release: same count, self-consistent
        # checksum, but Temple's names where Control's resources must be.
        legacy=bytearray((folder/'native-game.dat').read_bytes())
        replaced=0
        for at in range(24,24+len(expected_files)*80,80):
            name=bytes(legacy[at:at+64]).rstrip(b'\0')
            if name in (b'bg/bg_arec_all_p.seg',b'Tbg_arec_all_p_stanZ'):
                old=name.replace(b'arec',b'dish')
                legacy[at:at+64]=old.ljust(64,b'\0');replaced+=1
        assert replaced==2
        struct.pack_into('<I',legacy,16,fnv(legacy[24:]))
        (folder/'native-game.dat').write_bytes(legacy)
        legacy_render=bytearray((folder/'native-render.dat').read_bytes());legacy_render[:8]=b'EOGDRW16'
        (folder/'native-render.dat').write_bytes(legacy_render)
        subprocess.run([binary,folder],check=True,timeout=180)
        assert files((folder/'native-game.dat').read_bytes())==expected_files
        assert (folder/'native-render.dat').read_bytes()[:8]==b'EOGDRW21'
        assert config.read_text()=='preserve existing settings'
        (folder/'native-audio.pcm').write_bytes(b'incomplete')
        subprocess.run([binary,folder],check=True,timeout=60);assert (folder/'native-audio.pcm').read_bytes()==pcm
        assert not list(folder.glob('*.tmp'))
        (folder/'native-render.dat').write_bytes(b'incomplete')
        subprocess.run([binary,folder],check=True,timeout=180)
        assert (folder/'native-render.dat').read_bytes()[:8]==b'EOGDRW21'
    print('PASS console conversion: all '+str(len(expected_files))+' assets match desktop decoded bytes, exact audio cache, all 3 byte orders, SHA-1 rejection, cancellation, cache reuse and repair')
if __name__=='__main__':main()
