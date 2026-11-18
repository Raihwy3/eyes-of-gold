"""Recover original scalar defaults only between two verified data anchors.

The original N64 preprocessing determines declaration order. No port-only data
participates. Native record extents/alignment must reach the next exact anchor,
and complete scalar reconstruction must succeed before a row is accepted.
"""
import json,re,subprocess,zlib
from collections import Counter,defaultdict
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build,build_probe
from native_fixture import canonical_rom
from rom_data_plan import unit_name,declarations
BASE=0x80020d90


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());layouts=json.loads((build.BUILD/'original-data-types/layouts.json').read_text());sources={p.name:p for p in build.source_files()};by_unit=defaultdict(dict)
    for row in report['rows']:by_unit[unit_name(row['owner'])][row['symbol']]=row
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');raw=zlib.decompress(cart[0x21992:0x331e0],-15);changed=[]
    args=[a for a in build_probe.engine_args() if not str(a).startswith(('-DPORT=','-DPLATFORM_3DS=','-D__3DS__'))]
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        reference=ELFFile(f);sections=[s for s in reference.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for unit,rows in sorted(by_unit.items()):
            if not any(not r.get('bank') and '.' not in n for n,r in rows.items()):continue
            obj=build.BUILD/'original-data-types'/(unit+'.o')
            if not obj.exists() or unit not in sources:continue
            result=subprocess.run([build.CC,*args,'-E','-P',build.UP/sources[unit]],capture_output=True,text=True)
            if result.returncode:continue
            order=[]
            with obj.open('rb') as stream:
                e=ELFFile(stream);objects={s.name:s for s in e.get_section_by_name('.symtab').iter_symbols() if s['st_info']['type']=='STT_OBJECT' and s['st_size'] and isinstance(s['st_shndx'],int)}
                for first,last in declarations(result.stdout):
                    statement=result.stdout[first:last];left=statement.split('=',1)[0]
                    if re.search(r'\bconst\b',left):continue
                    m=re.search(r'\b(\w+)\s*(?:\[[^]]*\]\s*)*$',left)
                    if not m or m[1] not in objects:continue
                    symbol=objects[m[1]];section=e.get_section(symbol['st_shndx']);order.append((m[1],symbol['st_size'],section['sh_addralign']))
            def anchor(index):
                row=rows.get(order[index][0],{})
                return row.get('bank')=='cdata' and row.get('evidence') not in ('canonical complete duplicate scalar table','complete Lights1 byte-component record; alignment union is not a numeric u64 value') and row.get('source_bytes',row.get('bytes'))==row.get('bytes')
            for index,(name,size,alignment) in enumerate(order):
                row=rows.get(name);layout=layouts.get(unit,{}).get(name,{})
                if row is None or row.get('bank') or layout.get('size')!=size or not layout.get('fields') or row['bytes']!=size:continue
                left=next((i for i in range(index-1,-1,-1) if anchor(i)),None);right=next((i for i in range(index+1,len(order)) if anchor(i)),None)
                if left is None or right is None:continue
                cursor=rows[order[left][0]]['source_offset'];origins={}
                for candidate,extent,align in order[left:right+1]:
                    cursor=(cursor+align-1)&~(align-1);origins[candidate]=cursor;cursor+=extent
                if origins[order[right][0]]!=rows[order[right][0]]['source_offset']:continue
                offset=origins[name];section=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+size<=s['sh_addr']+s['sh_size']);at=row['address']-section['sh_addr'];native=section.data()[at:at+size];rebuilt=bytearray(size);fields=layout['fields']
                for off,width,kind in fields:rebuilt[off:off+width]=native[off:off+width] if kind=='pointer' else raw[offset+off:offset+off+width][::-1]
                if rebuilt!=native:continue
                row.update(bank='cdata',source_offset=offset,fields=fields,pointer_offsets=[o for o,w,k in fields if k=='pointer'],status='ROM span matched; relocation review required',evidence='original N64 declaration order between two exact data neighbours; full scalar reconstruction')
                row.pop('reason',None);changed.append(name)
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS bounded original declarations:',len(changed),changed)
if __name__=='__main__':main()
