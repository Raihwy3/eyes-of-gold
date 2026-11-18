"""Resolve duplicate animation definitions between exact original neighbours.

Identical payload copies do not establish pointer identity. Declaration order,
record extents, two independently matched neighbours and full scalar comparison
are all required before assigning an original address to the duplicate records.
"""
import json,re,zlib,struct
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom
BASE=0x80020d90


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());by_name={r['symbol']:r for r in report['rows'] if r['owner'].endswith('(chr.o)')};layouts=json.loads((build.BUILD/'original-data-types/layouts.json').read_text())['chr.c'];source=(build.UP/'src/game/chr.c').read_text()
    names=re.findall(r'struct\s+(?:weapon_firing_animation_table|anim_group_info)\s*\*?\s*(\w+)\s*(?:\[\s*\])?\s*=\s*\{',source)
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');raw=zlib.decompress(cart[0x21992:0x331e0],-15);changed=[]
    def anchor(row):return row.get('bank')=='cdata' and row.get('evidence')!='canonical complete duplicate scalar table'
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2];symbols=[s for s in e.get_section_by_name('.symtab').iter_symbols() if s['st_info']['type']=='STT_OBJECT' and s['st_size']]
        for index,name in enumerate(names):
            if name not in by_name:continue
            row=by_name[name]
            if row.get('bank') and row.get('evidence')!='canonical complete duplicate scalar table':continue
            left=next((i for i in range(index-1,-1,-1) if anchor(by_name.get(names[i],{}))),None);right=next((i for i in range(index+1,len(names)) if anchor(by_name.get(names[i],{}))),None)
            if left is None or right is None or any(n not in by_name for n in names[left:right+1]):continue
            cursor=by_name[names[left]]['source_offset'];origins={}
            for candidate in names[left:right+1]:
                cursor=(cursor+3)&~3;origins[candidate]=cursor;cursor+=by_name[candidate]['bytes']
            if origins[names[right]]!=by_name[names[right]]['source_offset']:continue
            offset=origins[name];fields=row.get('fields',layouts[name].get('fields'));assert fields is not None
            sec=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);at=row['address']-sec['sh_addr'];native=sec.data()[at:at+row['bytes']];rebuilt=bytearray(row['bytes']);valid=True
            for off,width,kind in fields:
                if kind=='pointer':
                    value=struct.unpack_from('<I',native,off)[0];key=struct.unpack_from('>I',raw,offset+off)[0]
                    target=next((s for s in symbols if s['st_value']==value),None)
                    if key or value:
                        if key==value:pass
                        elif target and target.name in origins:
                            if key!=BASE+origins[target.name]:valid=False;break
                        elif target and target.name in by_name and anchor(by_name[target.name]):
                            if key!=BASE+by_name[target.name]['source_offset']:valid=False;break
                        else:valid=False;break
                    rebuilt[off:off+width]=native[off:off+width]
                else:rebuilt[off:off+width]=raw[offset+off:offset+off+width][::-1]
            if valid and rebuilt==native:
                row.update(bank='cdata',source_offset=offset,fields=fields,pointer_offsets=[o for o,w,k in fields if k=='pointer'],status='ROM span matched; relocation review required',evidence='declaration order bounded by two exact original neighbours; full scalar and pointer identity reconstruction')
                row.pop('reason',None);changed.append(name)
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS bounded animation definitions:',len(changed),changed)
if __name__=='__main__':main()
