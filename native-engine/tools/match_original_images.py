"""Validate N64 image-entry bitfields against the native engine layout.

The format adapter repacks the four nibble fields using their declared N64
and native bit positions. Table contents are read solely from the user ROM.
"""
import json,zlib,struct
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom

def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text())
    row=next(r for r in report['rows'] if r['symbol']=='g_Textures')
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');cdata=zlib.decompress(rom[0x21992:0x331e0],-15)
    at=0x80049300-0x80020d90
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);s=next(s for s in e.iter_sections() if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);offset=row['address']-s['sh_addr'];native=s.data()[offset:offset+row['bytes']]
    rebuilt=bytearray();fields=[]
    assert len(native)%8==0
    for offset in range(0,len(native),8):
        first,second=struct.unpack_from('>II',cdata,at+offset)
        repacked=sum(((second>>(28-4*i))&15)<<(4*i) for i in range(4))
        rebuilt+=struct.pack('<II',first,repacked)
        fields.extend([[offset,4,'scalar'],[offset+4,4,'image_nibbles']])
    assert rebuilt==native,'Image-entry adapter does not reconstruct whole reference table'
    row.update(bank='cdata',source_offset=at,fields=fields,pointer_offsets=[],status='ROM span matched; relocation review required',evidence='documented N64/native image bitfield layout; all complete entries reconstruct native reference')
    row.pop('reason',None);report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS original image table:',len(native)//8,'entries,',len(native),'ROM-backed bytes')
if __name__=='__main__':main()
