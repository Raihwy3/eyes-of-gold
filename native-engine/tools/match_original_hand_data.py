"""Load the retail hand defaults, with empty native-only model state.

The original hand data ends at 0x3a8. The ARM adapter appends live model state
which has no serialized retail counterpart and must start zeroed. Do not read
that tail from the next unrelated ROM definition.
"""
import json,zlib
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text())
    layouts=json.loads((build.BUILD/'original-data-types/layouts.json').read_text())
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(cart[0x21992:0x331e0],-15)
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for name,unit,address in [('hand_data_dummy','initBondDATA.c',0x8002a3d0),('D_8003FDA0','unk_092E50.c',0x8003fda0)]:
            row=next(r for r in report['rows'] if r['symbol']==name)
            section=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);at=row['address']-section['sh_addr'];native=section.data()[at:at+row['bytes']]
            fields=[field for field in layouts[unit][name]['fields'] if field[0]+field[1]<=0x3a8]
            source=data[address-0x80020d90:address-0x80020d90+0x3a8];rebuilt=bytearray(row['bytes'])
            for offset,width,kind in fields:
                assert kind!='pointer' or not any(source[offset:offset+width]),'hand defaults unexpectedly contain a live pointer'
                rebuilt[offset:offset+width]=source[offset:offset+width][::-1]
            assert rebuilt==native,name+' whole native hand defaults do not match'
            row.update(bank='cdata',source_offset=address-0x80020d90,source_bytes=0x3a8,fields=fields,pointer_offsets=[o for o,w,k in fields if k=='pointer'],status='ROM span matched; relocation review required',evidence='retail hand prefix; native-only appended model state zeroed; complete reference reconstruction')
            row.pop('reason',None)
            print('PASS original hand defaults:',name,hex(address),len(source),'ROM bytes;',len(native)-len(source),'zero native-only bytes')
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
