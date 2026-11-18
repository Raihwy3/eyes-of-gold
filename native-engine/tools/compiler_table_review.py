"""Verify compiler-generated control-flow tables from actual saved assembly.

Names alone are not approval. Table entries must refer only to local labels in
one code section, and their emitted bytes/relocations must match the linked
input section. Literal arrays and string/data pointers remain unreviewed.
"""
import json,re,hashlib
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build
from rom_data_plan import unit_name
from review_port_data import normalizer


def assembly_tables(path):
    current='';labels={};blocks={}
    for line in path.read_text().splitlines():
        match=re.match(r'\s*\.section\s+([^,\s]+)',line)
        if match:current=match[1];continue
        match=re.match(r'\s*\.(text|data|bss)\b',line)
        if match:current='.'+match[1];continue
        match=re.match(r'\s*(\.L[\w.]+):',line)
        if match:labels[match[1]]=current
        if current.startswith('.rodata.'):
            match=re.match(r'\s*\.(byte|2byte|short|hword|word|long|ascii|asciz|string|zero|space|fill|quad|float|double)\s+(.*)',line)
            if match:blocks.setdefault(current,[]).append((match[1],match[2]))
    approved={}
    for section,entries in blocks.items():
        references=set();ok=True
        for kind,expression in entries:
            if kind in ('ascii','asciz','string','quad','float','double','fill'):ok=False;break
            if kind in ('zero','space'):
                if not re.fullmatch(r'\d+(?:\s*,\s*0)?',expression):ok=False;break
                continue
            for value in expression.split(','):
                value=value.strip()
                if value=='0':continue
                found=re.findall(r'\.L[\w.]+',value)
                rest=re.sub(r'\.L[\w.]+','',value)
                if not found or not re.fullmatch(r'[\s()*/+\-\d]*',rest):ok=False;break
                references.update(found)
            if not ok:break
        targets={labels.get(label,'') for label in references}
        if ok and references and len(targets)==1 and next(iter(targets)).startswith('.text.'):
            approved[section]=dict(target_section=next(iter(targets)),entries=len(entries),assembly_hash=hashlib.sha256(repr(entries).encode()).hexdigest())
    return approved


def review(elf,rows):
    paths={p.name:p for p in build.source_files()};cache={};approved=[];unresolved=[]
    with Path(elf).open('rb') as f:
        linked=ELFFile(f);get=normalizer(linked);functions={s.name:s for s in linked.get_section_by_name('.symtab').iter_symbols() if s['st_info']['type']=='STT_FUNC' and s['st_size']}
        for row in rows:
            unit=unit_name(row['owner']);rel=paths.get(unit)
            if rel is None:unresolved.append(row);continue
            obj=Path(row['owner']) if '/build/game/' in row['owner'] else build.BUILD/'obj'/rel.with_suffix('.o')
            if unit not in cache:
                assembly=obj.with_suffix('.s');cache[unit]=assembly_tables(assembly) if assembly.exists() else {}
            evidence=cache[unit].get(row['section'])
            if evidence is None or not obj.exists():unresolved.append(row);continue
            with obj.open('rb') as stream:
                original=ELFFile(stream);section=original.get_section_by_name(row['section'])
                if section is None or section['sh_size']!=row['bytes']:unresolved.append(row);continue
                expected=bytearray(section.data());pointer_offsets=[];valid=True
                for relocation_section in original.iter_sections():
                    if relocation_section['sh_type']!='SHT_REL' or original.get_section(relocation_section['sh_info']).name!=section.name:continue
                    for relocation in relocation_section.iter_relocations():
                        if relocation['r_info_type'] not in (2,38):valid=False;break
                        offset=relocation['r_offset'];expected[offset:offset+4]=bytes(4);pointer_offsets.append(offset)
                if not valid or bytes(expected)!=get(row['address'],row['bytes']):unresolved.append(row);continue
                # Absolute table entries must still land inside that function
                # after the linker applies their native address relocations.
                name=evidence['target_section'][6:];function=functions.get(name)
                if pointer_offsets:
                    target=linked.get_section(function['st_shndx']) if function is not None else None
                    data=next(s for s in linked.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size'])
                    at=row['address']-data['sh_addr'];raw=data.data()[at:at+row['bytes']]
                    if function is None or any(not function['st_value']<=int.from_bytes(raw[p:p+4],'little')<function['st_value']+function['st_size'] for p in pointer_offsets):unresolved.append(row);continue
            approved.append({**row,**evidence,'review':'compiler-generated native control-flow dispatch; exact object/link bytes and code labels verified'})
    (build.BUILD/'compiler-table-review.json').write_text(json.dumps(dict(reviewed=approved,unresolved=unresolved),indent=2)+'\n')
    return approved,unresolved
