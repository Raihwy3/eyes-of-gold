"""Import original menu records, then apply independently authored extensions.

Native control labels/options are custom code. All original labels, mappings
and default values are read from their original complete ROM records.
"""
import json,struct,zlib
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom,fnv
BASE=0x80020d90


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());rows={r['symbol']:r for r in report['rows']}
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(cart[0x21992:0x331e0],-15)
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for name,address,source_bytes in [('game_control_styles',0x800409dc,160),('game_options_entries',0x80040a7c,96),('D_800409D8',0x800409d8,4)]:
            row=rows[name];s=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);at=row['address']-s['sh_addr'];native=s.data()[at:at+row['bytes']];fields=[];raw=data[address-BASE:address-BASE+source_bytes]
            if name=='game_control_styles':
                assert row['bytes']==200
                fields=[[i,2,'scalar'] for i in range(0,160,2)]
                fields.extend([[dest+i,2,'scalar',i] for dest in (160,180) for i in range(0,20,2)])
            elif name=='game_options_entries':
                assert row['bytes']==144
                fields=[[offset+i,2,'scalar'] for offset in range(0,96,12) for i in range(0,8,2)]+[[offset+8,4,'scalar'] for offset in range(0,96,12)]
            else:fields=[[0,4,'scalar']]
            before=bytearray(row['bytes'])
            for field in fields:
                offset,width=field[:2];source=field[3] if len(field)>3 else offset
                before[offset:offset+width]=raw[source:source+width][::-1]
            after=bytearray(before)
            if name=='game_control_styles':
                struct.pack_into('<H',after,160,0xf000);struct.pack_into('<H',after,180,0xf001)
            elif name=='game_options_entries':
                for i in range(4):struct.pack_into('<HHHHI',after,96+i*12,0xf010+i,0,0,0,int(i>=2))
            else:struct.pack_into('<i',after,0,struct.unpack_from('<i',before)[0]+2)
            assert after==native,name+' original records plus native extension do not reconstruct the reference'
            row.update(bank='cdata',source_offset=address-BASE,source_bytes=source_bytes,fields=fields,pointer_offsets=[],pre_patch_hash=fnv(before),status='ROM span matched; relocation review required',evidence='complete retail menu records plus separately authored native menu extensions')
            row.pop('reason',None);print('PASS original/custom menu split:',name,source_bytes,'ROM bytes ->',row['bytes'],'native bytes')
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
