"""Import declared original C strings and the default weapon colour word.

Only complete declared string literals are matched; individual bytes are never
assembled into a payload recipe. Identical full strings may share a ROM copy.
"""
import ast,json,re,zlib
from collections import Counter
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build
from rom_data_plan import unit_name
from native_fixture import canonical_rom


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());layouts=json.loads((build.BUILD/'original-data-types/layouts.json').read_text());sources={Path(p).name:build.UP/p for p in json.loads((build.TARGET/'source-files.json').read_text())};cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(cart[0x21992:0x331e0],-15);count=0
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for row in report['rows']:
            if row.get('bank') or '.' in row['symbol']:continue
            name=row['symbol'];source=sources.get(unit_name(row['owner']))
            if source is None:continue
            sec=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);at=row['address']-sec['sh_addr'];native=sec.data()[at:at+row['bytes']]
            literal=re.search(r'\bchar\s+'+re.escape(name)+r'\s*\[\s*\]\s*=\s*("(?:\\.|[^"\\])*")\s*;',source.read_text())
            if literal:
                value=ast.literal_eval(literal[1]).encode('latin1')+b'\0'
                assert value==native,name+' literal/reference mismatch'
                offset=data.find(value)
                if offset<0:continue
                fields=[[i,1,'scalar'] for i in range(len(native))];evidence='complete declared string literal matched to authentic ROM string copy'
            elif re.search(r'\bLights1\s+'+re.escape(name)+r'\s*=',source.read_text()):
                assert len(native)==24
                offset=data.find(native)
                if offset<0:continue
                fields=[[i,1,'scalar'] for i in range(24)];evidence='complete Lights1 byte-component record; alignment union is not a numeric u64 value'
            elif name=='default_weaponstats':
                offset=0x80032494-0x80020d90;fields=[v for v in layouts['gun.c'][name]['fields'] if not 0x44<=v[0]<0x48]+[[0x44,4,'scalar']];evidence='default WeaponStats uses numeric 32-bit colour union member; complete native-byte reconstruction'
            else:continue
            rebuilt=bytearray(len(native))
            for off,width,kind in fields:rebuilt[off:off+width]=data[offset+off:offset+off+width][::-1]
            assert rebuilt==native,name+' whole reference mismatch'
            row.update(bank='cdata',source_offset=offset,fields=fields,pointer_offsets=[o for o,w,k in fields if k=='pointer'],status='ROM span matched; relocation review required',evidence=evidence)
            row.pop('reason',None);count+=1;print('PASS original small definition:',name,len(native),'bytes')
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('Additional complete original definitions:',count)
if __name__=='__main__':main()
