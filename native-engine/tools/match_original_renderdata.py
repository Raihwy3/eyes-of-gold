"""Import original render-state templates, adapting N64's four-byte bool.

ARM's _Bool occupies byte 0 of the aligned slot; the big-endian N64 true value
occupies byte 3. All other fields keep their declared 32-bit/byte layouts.
"""
import json,zlib
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());names={'cast_renderdata_default','unknown_folderselect','gunbarrelRenderData','headModelRenderData','goldeneyelogo_MRD','nintendologo_MRD','legalscreen_MRD'}
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(cart[0x21992:0x331e0],-15)
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for row in report['rows']:
            if row['symbol'] not in names:continue
            assert row['bytes']==64
            sec=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+64<=s['sh_addr']+s['sh_size']);at=row['address']-sec['sh_addr'];native=sec.data()[at:at+64]
            fields=[[0,4,'pointer'],[4,1,'scalar',7],[8,4,'scalar'],[12,4,'pointer'],[16,4,'pointer']]+[[i,4,'scalar'] for i in range(20,52,4)]+[[i,1,'scalar'] for i in range(52,60)]+[[60,4,'scalar']]
            expected=bytearray(64)
            for field in fields:
                offset,width,kind=field[:3];source=field[3] if len(field)>3 else offset
                expected[source:source+width]=native[offset:offset+width][::-1]
            assert not any(native[5:8]),'unexpected native bool padding'
            assert not any(native[0:4]+native[12:20]),'non-null template pointer needs address review'
            matches=[];position=0
            while (position:=data.find(expected,position))>=0:
                if position%4==0:matches.append(position)
                position+=1
            assert matches,row['symbol']+' no whole original template'
            row.update(bank='cdata',source_offset=matches[0],fields=fields,pointer_offsets=[0,12,16],pointer_targets={},status='ROM span matched; relocation review required',evidence='canonical complete duplicate scalar table')
            row.pop('reason',None);print('PASS original render-state template:',row['symbol'],'N64 bool slot adapted; whole template exact')
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
