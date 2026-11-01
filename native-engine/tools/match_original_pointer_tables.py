"""Resolve complete pointer tables against known original/native ABI addresses.

No pointer ordering or scalar payload is emitted. Only a unique complete ROM
span whose fields reconstruct the reference native object is accepted.
"""
import json,struct,zlib
from collections import defaultdict,Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom
from rom_data_plan import unit_name


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text())
    layouts=json.loads((build.BUILD/'original-data-types/layouts.json').read_text())
    plan=json.loads((build.BUILD/'original-pointer-plan.json').read_text());getters={g['getter']:g for g in plan['getters']}
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(rom[0x21992:0x331e0],-15)
    changed=[]
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        symbols={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols() if s['st_info']['bind']!='STB_LOCAL'}
        absolute=set()
        for sec in e.iter_sections():
            if sec['sh_type']!='SHT_REL':continue
            target=e.get_section(sec['sh_info'])
            if target['sh_flags']&2 and not target['sh_flags']&4:
                absolute.update(r['r_offset'] for r in sec.iter_relocations() if r['r_info_type'] in (2,38))
        def native_bytes(address,size):
            sec=next((s for s in sections if s['sh_addr']<=address and address+size<=s['sh_addr']+s['sh_size']),None)
            if sec:return sec.data()[address-sec['sh_addr']:address-sec['sh_addr']+size]
        native_to_keys=defaultdict(set)
        for p in plan['pointers']:
            if p['kind']=='symbol':native=symbols.get(getters[p['getter']]['name'],0)+p.get('addend',0)
            elif p['kind']=='identity':native=p['key']
            elif p['kind']=='cart':native=p['key']+0x10000000
            elif p['kind']=='segment_offset':native=p['key']&0xffffff
            else:continue
            native_to_keys[native].add(p['key'])
        # Matched objects establish their own original base even if no other
        # initialized table points to them yet. This unlocks pointer-only roots.
        # Already-reviewed pointer fields also bind merged string literals,
        # which have no native object symbol. IDO and GCC merge strings
        # differently, so retain every exact complete cdata string copy.
        for linked in plan['rows']:
            source=data if linked['bank']=='cdata' else rom
            raw=native_bytes(linked['address'],linked['bytes'])
            for offset in linked['pointer_offsets']:
                if str(offset) in linked.get('pointer_targets',{}):continue
                key=struct.unpack_from('>I',source,linked['source_offset']+offset)[0]
                value=struct.unpack_from('<I',raw,offset)[0]
                if not key:continue
                native_to_keys[value].add(key)
                if 0x80020d90<=key<0x80020d90+len(data):
                    start=key-0x80020d90;end=data.find(b'\0',start)
                    text=data[start:end+1] if end>=start else b''
                    if 2<len(text)<=128 and all(c in (9,10,13) or 32<=c<127 for c in text[:-1]) and native_bytes(value,len(text))==text:
                        pos=0
                        while (pos:=data.find(text,pos))>=0:
                            native_to_keys[value].add(0x80020d90+pos);pos+=1
        anchors=[r for r in report['rows'] if r.get('bank')=='cdata' and r.get('evidence')!='canonical complete duplicate scalar table']
        for anchor in anchors:
            native_to_keys[anchor['address']].add(0x80020d90+anchor['source_offset'])
        # Odd-texture command programs occupy N64 segment 2. Their whole
        # command payloads have already matched the source ROM extents.
        for anchor in report['rows']:
            if anchor.get('bank')=='rom' and unit_name(anchor['owner'])=='oddtextures.c':
                native_to_keys[anchor['address']].add(0x02000000+anchor['source_offset']-0x29d160)
        for row in report['rows']:
            if row.get('bank') or '.' in row['symbol']:continue
            layout=layouts.get(unit_name(row['owner']),{}).get(row['symbol'],{})
            if 'fields' not in layout or layout.get('size')!=row['bytes']:continue
            native=native_bytes(row['address'],row['bytes']);fields=[];known=[]
            for at,width,kind in layout['fields']:
                if width==4 and row['address']+at in absolute:kind='pointer'
                fields.append([at,width,kind])
                if kind=='pointer':
                    value=struct.unpack_from('<I',native,at)[0]
                    if value in native_to_keys:known.append((at,native_to_keys[value]))
            if not known:
                # A pointer-only root may be the sole user of a literal pool.
                # Use complete printable native strings as anchors; every
                # pointer and scalar still has to validate across the table.
                for at,width,kind in fields:
                    if kind!='pointer':continue
                    value=struct.unpack_from('<I',native,at)[0]
                    section=next((s for s in sections if s['sh_addr']<=value<s['sh_addr']+s['sh_size']),None)
                    if section is None:continue
                    offset=value-section['sh_addr'];tail=section.data()[offset:offset+4096];end=tail.find(b'\0')
                    if end<3 or not all(c in (9,10,13) or 32<=c<127 for c in tail[:end]):continue
                    text=tail[:end+1];keys=set();position=0
                    while (position:=data.find(text,position))>=0:
                        keys.add(0x80020d90+position);position+=1
                    if keys:known.append((at,keys))
            if not known:continue
            at,keys=min(known,key=lambda x:len(x[1]));candidates=set()
            for key in keys:
                needle=struct.pack('>I',key);position=0
                while (position:=data.find(needle,position))>=0:
                    start=position-at;position+=1
                    if start>=0 and start+row['bytes']<=len(data):candidates.add(start)
            valid=[]
            for start in sorted(candidates):
                rebuilt=bytearray(row['bytes']);ok=True
                for at,width,kind in fields:
                    source=data[start+at:start+at+width]
                    if kind=='pointer':
                        key=struct.unpack('>I',source)[0];value=struct.unpack_from('<I',native,at)[0]
                        if key==value==0:pass
                        elif key in native_to_keys.get(value,()):pass
                        elif 0x80020d90<=key<0x80020d90+len(data):
                            offset=key-0x80020d90;end=data.find(b'\0',offset)
                            if end<offset or end-offset>4096 or native_bytes(value,end-offset+1)!=data[offset:end+1]:ok=False;break
                        else:ok=False;break
                        rebuilt[at:at+width]=native[at:at+width]
                    else:rebuilt[at:at+width]=source[::-1]
                if ok and rebuilt==native:valid.append(start)
            if len(valid)==1:
                row.update(bank='cdata',source_offset=valid[0],fields=fields,pointer_offsets=[a for a,_,k in fields if k=='pointer'],status='ROM span matched; relocation review required',evidence='unique complete scalar and original/native pointer table reconstruction')
                row.pop('reason',None);changed.append(row['symbol'])
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('Complete original pointer tables:',len(changed),'additional objects');print(changed)

if __name__=='__main__':main()
