"""Resolve original union layouts only against authentic complete ROM spans.

Original pointer references provide data addresses when source comments are
missing. Unsupported fields and mismatches stay unapproved.
"""
import json,struct,zlib,re
from pathlib import Path
from collections import defaultdict,Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom
from original_data_types import type_size
from rom_data_plan import unit_name
BASE=0x80020d90

def fields_at(die,native,source,relocs,base=0,root_size=None):
    tag=die.tag;a=die.attributes
    if tag in ('DW_TAG_const_type','DW_TAG_volatile_type','DW_TAG_typedef','DW_TAG_restrict_type'):
        return fields_at(die.get_DIE_from_attribute('DW_AT_type'),native,source,relocs,base,root_size)
    if tag=='DW_TAG_pointer_type':return [(base,4,'pointer')]
    if tag in ('DW_TAG_base_type','DW_TAG_enumeration_type'):
        size=a['DW_AT_byte_size'].value
        if base in relocs and size==4:return [(base,4,'pointer')]
        if any(base<=p<base+size for p in relocs):raise ValueError('pointer overlaps scalar representation')
        if native[base:base+size]!=source[base:base+size][::-1]:raise ValueError('scalar mismatch at '+hex(base)+': native '+native[base:base+size].hex()+' / ROM '+source[base:base+size].hex())
        return [(base,size,'scalar')]
    if tag=='DW_TAG_array_type':
        element=die.get_DIE_from_attribute('DW_AT_type');size=type_size(element);count=1
        for child in die.iter_children():
            if child.tag!='DW_TAG_subrange_type':continue
            ca=child.attributes
            if 'DW_AT_count' in ca:n=ca['DW_AT_count'].value
            elif 'DW_AT_upper_bound' in ca:n=ca['DW_AT_upper_bound'].value+1
            elif root_size is not None:n=root_size//size
            else:raise ValueError('unknown array bound')
            if not isinstance(n,int) or n<0:raise ValueError('nonconstant bound')
            count*=n
        result=[]
        for i in range(count):result+=fields_at(element,native,source,relocs,base+i*size)
        return result
    if tag=='DW_TAG_union_type':
        options=[]
        for child in die.iter_children():
            if child.tag!='DW_TAG_member':continue
            try:options.append(fields_at(child.get_DIE_from_attribute('DW_AT_type'),native,source,relocs,base))
            except ValueError:pass
        if not options:raise ValueError('no union representation matches ROM/native bytes')
        options.sort(key=lambda fields:(-sum(w for _,w,_ in fields),sum(k=='pointer' for _,_,k in fields)))
        return options[0]
    if tag=='DW_TAG_structure_type':
        result=[]
        for child in die.iter_children():
            if child.tag!='DW_TAG_member':continue
            ca=child.attributes
            if 'DW_AT_bit_size' in ca:raise ValueError('bitfields require explicit conversion')
            offset=ca.get('DW_AT_data_member_location');offset=offset.value if offset else 0
            if not isinstance(offset,int):raise ValueError('nonconstant member location')
            result+=fields_at(child.get_DIE_from_attribute('DW_AT_type'),native,source,relocs,base+offset)
        return result
    raise ValueError(tag)

def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());pointer=json.loads((build.BUILD/'original-pointer-plan.json').read_text());getters={g['getter']:g for g in pointer['getters']};origins=defaultdict(set)
    for p in pointer['pointers']:
        if p['kind']=='symbol':origins[getters[p['getter']]['name']].add(p['key']-p.get('addend',0))
    # A named N64 address is also a valid origin. It still has to pass
    # complete typed scalar reconstruction and native pointer review.
    for row in report['rows']:
        label=re.search(r'(?:D_|asc_D_)(800[0-9a-fA-F]{5})',row['symbol'])
        if label:origins[row['symbol']].add(int(label[1],16))
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');cdata=zlib.decompress(rom[0x21992:0x331e0],-15)
    groups=defaultdict(list)
    for row in report['rows']:
        if not row.get('bank') and row['symbol'] in origins:groups[unit_name(row['owner'])].append(row)
    changed=[];failures=[]
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as stream:
        e=ELFFile(stream);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2];absolute=set()
        for section in e.iter_sections():
            if section['sh_type']!='SHT_REL':continue
            target=e.get_section(section['sh_info'])
            if not target['sh_flags']&2 or target['sh_flags']&4:continue
            for relocation in section.iter_relocations():
                if relocation['r_info_type'] in (2,38):absolute.add(relocation['r_offset'])
        for unit,rows in groups.items():
            obj=build.BUILD/'original-data-types'/(unit+'.o')
            with obj.open('rb') as f:
                dwarf=ELFFile(f).get_dwarf_info();types={}
                for cu in dwarf.iter_CUs():
                    for die in cu.iter_DIEs():
                        if die.tag!='DW_TAG_variable':continue
                        spec=die.get_DIE_from_attribute('DW_AT_specification') if 'DW_AT_specification' in die.attributes else None
                        a={**(spec.attributes if spec else {}),**die.attributes}
                        if 'DW_AT_name' not in a or 'DW_AT_type' not in a:continue
                        name=a['DW_AT_name'].value.decode()
                        if name not in types or 'DW_AT_location' in die.attributes:types[name]=(die if 'DW_AT_type' in die.attributes else spec).get_DIE_from_attribute('DW_AT_type')
                for row in rows:
                    name=row['symbol'];matches=[];error='no compatible authentic address'
                    section=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);at=row['address']-section['sh_addr'];native=section.data()[at:at+row['bytes']]
                    for address in origins[name]:
                        if not BASE<=address<=BASE+len(cdata)-len(native):continue
                        source=cdata[address-BASE:address-BASE+len(native)]
                        try:
                            fields=fields_at(types[name],native,source,{p-row['address'] for p in absolute if row['address']<=p<row['address']+row['bytes']},root_size=len(native));rebuilt=bytearray(len(native))
                            for offset,width,kind in fields:
                                rebuilt[offset:offset+width]=native[offset:offset+width] if kind=='pointer' else source[offset:offset+width][::-1]
                            if rebuilt!=native:raise ValueError('uncovered nonzero bytes')
                            matches.append((address,fields))
                        except (ValueError,KeyError) as ex:error=str(ex)
                    if len(matches)==1:
                        address,fields=matches[0];row.update(bank='cdata',source_offset=address-BASE,fields=fields,pointer_offsets=[at for at,_,kind in fields if kind=='pointer'],status='ROM span matched; relocation review required',evidence='original pointer address and complete native-byte reconstruction')
                        row.pop('reason',None);changed.append(name)
                    else:failures.append(dict(symbol=name,reason=error if not matches else 'multiple authentic origins'))
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    (build.BUILD/'original-union-review.json').write_text(json.dumps(dict(resolved=changed,unresolved=failures),indent=2)+'\n')
    print('Authentic pointer/type reconstruction:',len(changed),'additional objects;',len(failures),'remain for review');print(Counter(r['reason'] for r in failures))
if __name__=='__main__':main()
