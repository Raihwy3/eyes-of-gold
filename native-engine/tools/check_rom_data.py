"""Exercise actual runtime ROM table loading and native pointer selection."""
import json,os,struct,subprocess,tempfile
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build,rom_data_plan
from native_fixture import canonical_rom

def main():
    plan=rom_data_plan.runtime_plan();rom_data_plan.emit_runtime_header()
    out=build.BUILD/'rom-data-host';out.mkdir(exist_ok=True)
    native_symbols={r['link_name']:r for r in plan['rows']}
    definitions=['#include <stdint.h>']
    for row in plan['rows']:definitions.append('unsigned char '+row['link_name']+'['+str(row['bytes'])+'] __attribute__((aligned(8)));')
    definitions.append('const void *native_rom_test_symbol(unsigned index){switch(index){')
    for i,g in enumerate(plan['getters']):
        target='(const void *)'+g['name'] if g['name'] in native_symbols else '(const void *)(uintptr_t)'+str(0x02000000+i*4)+'u'
        definitions.append('case '+str(i)+':return '+target+';')
    definitions+=['default:return 0;}}','#define NATIVE_ROM_DATA_TEST','#include "'+str(build.TARGET/'src/native_rom_data.c')+'"']
    definitions.append(r'''
int main(int argc,char **argv){if(argc!=3)return 2;
 if(!native_rom_data_init(argv[1])){fprintf(stderr,"%s\n",native_rom_data_error());return 1;}
 FILE *out=fopen(argv[2],"wb");if(!out)return 2;
 uint32_t base=(uintptr_t)original_cdata;fwrite(&base,4,1,out);
 for(unsigned i=0;i<native_rom_data_count();i++){const struct NativeRomObject *r=&native_rom_objects[i];uint32_t address=(uintptr_t)r->destination;fwrite(&address,4,1,out);fwrite(r->destination,1,r->bytes,out);}
 fclose(out);
 unsigned char old=native_rom_objects[0].destination[0];native_rom_objects[0].destination[0]^=1;
 if(!native_rom_data_init(argv[1])||native_rom_objects[0].destination[0]==old)return 3;
 return 0;
}
''')
    source=out/'check.c';source.write_text('\n'.join(definitions))
    shim=out/'inflate.o';flags=['-O2','-g','-std=c11','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include')]
    if os.getenv('EOG_ROM_DATA_SANITIZE'):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run(['gcc',*flags,'-DCONVERT_NO_MAIN','-c',build.TARGET/'tests/native_convert_host.c','-o',shim],check=True)
    binary=out/'check';subprocess.run(['gcc',*flags,'-no-pie',source,shim,'-lz','-o',binary],check=True)
    pointer={p['key']:p for p in plan['pointers']};getters={g['getter']:(i,g) for i,g in enumerate(plan['getters'])}
    with tempfile.TemporaryDirectory(dir=out) as name:
        folder=Path(name);cart=folder/'native-cart.bin';dump=folder/'dump.dat'
        assert subprocess.run([binary,folder,dump],capture_output=True).returncode==1
        rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');cart.write_bytes(rom)
        subprocess.run([binary,folder,dump],check=True);data=dump.read_bytes();base=struct.unpack_from('<I',data)[0];at=4;actual={};addresses={}
        for row in plan['rows']:
            addresses[row['link_name']]=struct.unpack_from('<I',data,at)[0];at+=4;actual[row['link_name']]=data[at:at+row['bytes']];at+=row['bytes']
        assert at==len(data)
        def resolve(key):
            if not key:return 0
            p=pointer[key];kind=p['kind']
            if kind=='identity':return key
            if kind=='cart':return key+0x10000000
            if kind=='segment_offset':return key&0xffffff
            if kind=='cdata_address':return base+p['offset']
            i,g=getters[p['getter']];return addresses.get(g['name'],0x02000000+i*4)+p.get('addend',0)
        import zlib
        cdata=zlib.decompress(rom[0x21992:0x331e0],-15)
        audit=json.loads((build.BUILD/'original-data-matches.json').read_text());references={(r['symbol'],Path(r['owner'].split('(')[-1].rstrip(')')).name):r for r in audit['rows']}
        with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
            elf=ELFFile(f);sections=[s for s in elf.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
            for row in plan['rows']:
                if row.get('storage') in ('zeroed-string-pool','zeroed-compiler-pool'):
                    source=rom if row['bank']=='rom' else cdata;expected=bytearray(row['bytes'])
                    for field in row['fields']:
                        offset,width,kind=field[:3];start=field[3] if len(field)>3 else offset
                        assert kind!='pointer' or not any(source[row['source_offset']+start:row['source_offset']+start+width])
                        value=source[row['source_offset']+start:row['source_offset']+start+width]
                        expected[offset:offset+width]=(int.from_bytes(value,'big')&~16).to_bytes(width,'little') if kind=='native_save_options' else value[::-1]
                    from native_fixture import fnv
                    assert fnv(expected)==row['hash'] and actual[row['link_name']]==expected,row['symbol']+' ROM compiler-pool mismatch'
                    continue
                reference=next(r for r in audit['rows'] if r['symbol']==row['symbol'] and (Path(r['owner']).name==row['unit']+'.o' or r['owner'].endswith('('+row['unit'][:-2]+'.o)')))
                section=next(s for s in sections if s['sh_addr']<=reference['address'] and reference['address']+row['bytes']<=s['sh_addr']+s['sh_size']);offset=reference['address']-section['sh_addr'];expected=bytearray(section.data()[offset:offset+row['bytes']])
                source=rom if row['bank']=='rom' else cdata
                for field in row['fields']:
                    offset,width,kind=field[:3];input_at=field[3] if len(field)>3 else offset
                    if kind=='pointer':struct.pack_into('<I',expected,offset,resolve(struct.unpack_from('>I',source,row['source_offset']+input_at)[0]))
                assert actual[row['link_name']]==expected,(row['symbol'],'scalar or pointer mismatch')
        invalid=bytearray(rom);invalid[0x21990]^=1;cart.write_bytes(invalid)
        assert subprocess.run([binary,folder,dump],capture_output=True).returncode==1
    print('PASS original ROM data loader:',len(plan['rows']),'objects; exact scalars, native pointer targets, ROM strings, missing/corrupt source rejection and startup-only initialization')
if __name__=='__main__':main()
