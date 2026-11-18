"""Build native relocation metadata for original ROM pointer fields.

The ROM supplies pointer selection/order. This dictionary maps N64 addresses to
native symbols or ROM strings. It contains no original text/definition payload.
Conflicting or unresolvable mappings are held for review rather than guessed.
"""
import bisect,json,re,struct,zlib,hashlib
from pathlib import Path
from collections import defaultdict,Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom
from rom_data_plan import unit_name

def generate():
    report=json.loads((build.BUILD/'original-data-matches.json').read_text());rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');cdata=zlib.decompress(rom[0x21992:0x331e0],-15)
    text=(build.BUILD/'original-data-reference/eyes-of-gold-native-game.map').read_text();ranges=[]
    for m in re.finditer(r'^ (\.(?:text|data|rodata|bss)(?:\.[^\s]+)?)\s*(?:\n\s*)?(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+)\s+([^\n]+)',text,re.M):
        section,at,size,owner=m.groups();at,size=int(at,16),int(size,16)
        if at and size:ranges.append((at,at+size,owner))
    getters={};mappings=defaultdict(dict);rows=[];failed=[]
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as stream:
        elf=ELFFile(stream);sections=[s for s in elf.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        symbols=[s for s in elf.get_section_by_name('.symtab').iter_symbols() if s['st_info']['type'] in ('STT_OBJECT','STT_FUNC') and s['st_size'] and isinstance(s['st_shndx'],int)]
        by_name={s.name:s for s in symbols if s['st_info']['bind']!='STB_LOCAL'}
        symbols.sort(key=lambda s:s['st_value']);starts=[s['st_value'] for s in symbols]
        def native_bytes(at,size):
            sec=next((s for s in sections if s['sh_addr']<=at and at+size<=s['sh_addr']+s['sh_size']),None)
            return sec.data()[at-sec['sh_addr']:at-sec['sh_addr']+size] if sec else None
        def target(key,value):
            if not key and not value:return dict(kind='null')
            if not key:raise ValueError('native port pointer replaces original null')
            if value==key:return dict(kind='identity')
            if key<0xc00000 and value==key+0x10000000:return dict(kind='cart')
            if key&0xff000000==0x04000000 and value==key&0xffffff:return dict(kind='segment_offset')
            index=bisect.bisect_right(starts,value)
            selected=[s for s in symbols[max(0,index-5):index] if s['st_value']<=value<s['st_value']+s['st_size']]
            selected.sort(key=lambda s:(s['st_size'],-s['st_value']))
            for symbol in selected:
                name=symbol.name;addend=value-symbol['st_value']
                if not re.fullmatch(r'[A-Za-z_]\w*',name):continue
                unit=None
                if symbol['st_info']['bind']=='STB_LOCAL':
                    owner=next((r[2] for r in ranges if r[0]<=symbol['st_value']<r[1]),None)
                    if not owner:continue
                    unit=unit_name(owner)
                    if unit not in {Path(p).name for p in json.loads((build.TARGET/'source-files.json').read_text())}:continue
                uid=hashlib.sha256((str(unit)+':'+name).encode()).hexdigest()[:16]
                getters[uid]=dict(name=name,unit=unit,getter='eog_rom_ref_'+uid)
                return dict(kind='symbol',getter=getters[uid]['getter'],addend=addend)
            # Literal strings merged by the C compiler have no object symbol.
            if 0x80020d90<=key<0x80020d90+len(cdata):
                at=key-0x80020d90;end=cdata.find(b'\0',at)
                if end>=0 and end-at<=4096:
                    actual=native_bytes(value,end-at+1)
                    if actual==cdata[at:end+1]:return dict(kind='cdata_address',offset=at)
            raise ValueError('unresolved native pointer '+hex(value)+' / N64 '+hex(key))
        for row in report['rows']:
            if not row.get('bank') or not row['pointer_offsets'] or '.' in row['symbol']:continue
            source=cdata if row['bank']=='cdata' else rom;native=native_bytes(row['address'],row['bytes']);entries=[]
            try:
                for at in row['pointer_offsets']:
                    key=struct.unpack_from('>I',source,row['source_offset']+at)[0];value=struct.unpack_from('<I',native,at)[0]
                    override=row.get('pointer_targets',{}).get(str(at))
                    if override:value=by_name[override]['st_value']
                    resolved=target(key,value)
                    entries.append((key,resolved))
                rows.append(row)
                for key,resolved in entries:
                    if resolved['kind']!='null':mappings[key][json.dumps(resolved,sort_keys=True)]=resolved
            except ValueError as error:failed.append(dict(symbol=row['symbol'],reason=str(error)))
        conflicts={key for key,values in mappings.items() if len(values)!=1}
        accepted=[]
        for row in rows:
            source=cdata if row['bank']=='cdata' else rom
            if any(struct.unpack_from('>I',source,row['source_offset']+at)[0] in conflicts for at in row['pointer_offsets']):
                failed.append(dict(symbol=row['symbol'],reason='shared N64 pointer mapping conflicts'));continue
            accepted.append(row)
        used=set()
        for row in accepted:
            source=cdata if row['bank']=='cdata' else rom
            used.update(struct.unpack_from('>I',source,row['source_offset']+at)[0] for at in row['pointer_offsets'])
        pointers=[dict(key=key,**next(iter(mappings[key].values()))) for key in sorted(used) if key]
        getter_names={r['getter'] for r in pointers if r['kind']=='symbol'}
        result=dict(conflicts={str(key):list(mappings[key].values()) for key in conflicts},rows=accepted,pointers=pointers,getters=[r for r in getters.values() if r['getter'] in getter_names],unresolved=failed)
        (build.BUILD/'original-pointer-plan.json').write_text(json.dumps(result,indent=2)+'\n')
        print('ROM pointer plan:',len(accepted),'objects;',len(pointers),'relocations;',len(failed),'objects need review; conflicts',len(conflicts))
        print(Counter(x['reason'].split(' / ')[0] for x in failed))
        return result
if __name__=='__main__':generate()
