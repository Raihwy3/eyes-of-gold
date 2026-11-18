"""Inventory live initialized objects by linker provenance, not ZIP extensions.

A release cannot be described as ROM-sourced merely because .dat files are not
in the ZIP. Original engine data and anonymous strings need their own review.
The JSON report is deliberately conservative: unknown objects stay unresolved.
"""
import json,re
from pathlib import Path
from collections import Counter
from elftools.elf.elffile import ELFFile
import build

def inventory(elf=None,mapfile=None):
    elf=elf or build.BUILD/'eyes-of-gold-native-game.elf'
    mapfile=mapfile or elf.with_suffix('.map')
    text=mapfile.read_text();ranges=[]
    pattern=r'^ (\.(?:data|rodata)(?:\.[^\s]+)?)\s*(?:\n\s*)?(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+)\s+([^\n]+)'
    for m in re.finditer(pattern,text,re.M):
        section,at,size,owner=m.groups();at,size=int(at,16),int(size,16)
        if at and size:ranges.append((at,at+size,owner,section))
    rows=[]
    with elf.open('rb') as stream:
        e=ELFFile(stream)
        for symbol in e.get_section_by_name('.symtab').iter_symbols():
            if symbol['st_info']['type']!='STT_OBJECT' or not isinstance(symbol['st_shndx'],int) or not symbol['st_size']:continue
            sec=e.get_section(symbol['st_shndx'])
            if sec['sh_type']!='SHT_PROGBITS' or not sec['sh_flags']&2:continue
            at=symbol['st_value'];owner=next((r for r in ranges if r[0]<=at and at+symbol['st_size']<=r[1]),None)
            if not owner:continue
            source=owner[2];original='liboriginal-engine.a(' in source
            if '/build/game/' in source:
                name=Path(source).name
                original=name in {Path(r).name+'.o' for r in json.loads((build.TARGET/'source-files.json').read_text())}
            if not original:continue
            rows.append(dict(symbol=symbol.name,address=at,bytes=symbol['st_size'],owner=source,section=owner[3],status='unresolved'))
    return rows

def anonymous_inventory(elf,mapfile=None):
    """Original initialized section bytes not covered by a named object."""
    mapfile=mapfile or elf.with_suffix('.map');text=mapfile.read_text();rows=[]
    source_names={Path(r).name+'.o' for r in json.loads((build.TARGET/'source-files.json').read_text())}
    pattern=r'^ (\.(?:data|rodata)(?:\.[^\s]+)?)\s*(?:\n\s*)?(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+)\s+([^\n]+)'
    with elf.open('rb') as f:
        e=ELFFile(f);symbols=[s for s in e.get_section_by_name('.symtab').iter_symbols() if s['st_info']['type']=='STT_OBJECT' and s['st_size']]
        sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for match in re.finditer(pattern,text,re.M):
            name,address,size,owner=match.groups();address,size=int(address,16),int(size,16)
            original='liboriginal-engine.a(' in owner or ('/build/game/' in owner and Path(owner).name in source_names)
            if not original or not address or not size:continue
            section=next((s for s in sections if s['sh_addr']<=address and address+size<=s['sh_addr']+s['sh_size']),None)
            if not section:continue
            at=address-section['sh_addr'];data=section.data()[at:at+size];covered=bytearray(size)
            for symbol in symbols:
                lo=max(address,symbol['st_value']);hi=min(address+size,symbol['st_value']+symbol['st_size'])
                if hi>lo:covered[lo-address:hi-address]=b'\1'*(hi-lo)
            pending=sum(bool(value) and not covered[i] for i,value in enumerate(data))
            if pending:rows.append(dict(owner=owner,section=name,address=address,bytes=size,unreviewed_nonzero_bytes=pending,status='unresolved anonymous data'))
    return rows


def require_clean_release(elf,mapfile=None):
    import hashlib
    from review_port_data import normalizer
    from rom_data_plan import unit_name, runtime_plan
    elf=Path(elf);mapfile=Path(mapfile) if mapfile else elf.with_suffix('.map')
    rows=inventory(elf,mapfile);unresolved=[];reviewed=[];storage_errors=[]
    policy_path=build.TARGET/'rom-data-code-review.json'
    policy=json.loads(policy_path.read_text()) if policy_path.exists() else dict(rows=[])
    if policy.get('upstream',build.UPSTREAM_REV)!=build.UPSTREAM_REV:raise RuntimeError('Code-data provenance revision mismatch')
    approvals={(r['unit'],r['symbol']):r for r in policy['rows']}
    manifest=runtime_plan()
    with elf.open('rb') as f:
        e=ELFFile(f);get=normalizer(e);symbols={s.name:s for s in e.get_section_by_name('.symtab').iter_symbols()}
        sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        def raw_bytes(address,size):
            section=next(s for s in sections if s['sh_addr']<=address and address+size<=s['sh_addr']+s['sh_size'])
            at=address-section['sh_addr'];return section.data()[at:at+size]
        imported_ranges=[]
        for item in manifest['rows']:
            symbol=symbols.get(item['link_name'])
            if symbol:imported_ranges.append((symbol['st_value'],symbol['st_value']+item['bytes']))
        for row in rows:
            if any(lo<=row['address'] and row['address']+row['bytes']<=hi for lo,hi in imported_ranges):continue
            actual=get(row['address'],row['bytes']);approval=approvals.get((unit_name(row['owner']),row['symbol']))
            source_valid=approval is not None and all((build.ROOT/path).exists() and hashlib.sha256((build.ROOT/path).read_bytes()).hexdigest()==value for path,value in approval.get('source_sha256',{}).items())
            if source_valid and row['bytes']==approval['bytes'] and hashlib.sha256(actual).hexdigest()==approval['sha256']:
                reviewed.append({**row,'review':approval['review'],'evidence':approval['evidence']})
            else:unresolved.append(row)
        for item in manifest['rows']:
            symbol=symbols.get(item['link_name'])
            valid=symbol is not None and symbol['st_size']==item['bytes'] and isinstance(symbol['st_shndx'],int)
            if valid:
                section=e.get_section(symbol['st_shndx'])
                valid=section['sh_type']=='SHT_NOBITS'
                if item.get('storage') in ('zeroed-string-pool','zeroed-compiler-pool'):
                    valid=section['sh_type']=='SHT_PROGBITS' and bool(section['sh_flags']&1) and not any(raw_bytes(symbol['st_value'],item['bytes']))
            if not valid:storage_errors.append(item['symbol'])
    anonymous=anonymous_inventory(elf,mapfile)
    from compiler_table_review import review as review_compiler_tables
    compiler_tables,anonymous=review_compiler_tables(elf,anonymous)
    pool_path=build.TARGET/'rom-code-pool-review.json'
    pool_policy=json.loads(pool_path.read_text()) if pool_path.exists() else dict(rows=[])
    if pool_policy.get('upstream',build.UPSTREAM_REV)!=build.UPSTREAM_REV:raise RuntimeError('Code-pool provenance revision mismatch')
    pools={(r['unit'],r['section']):r for r in pool_policy['rows']};remaining=[];reviewed_pools=[]
    with elf.open('rb') as f:
        get=normalizer(ELFFile(f))
        for row in anonymous:
            approval=pools.get((unit_name(row['owner']),row['section']))
            source_valid=approval is not None and all((build.ROOT/path).exists() and hashlib.sha256((build.ROOT/path).read_bytes()).hexdigest()==value for path,value in approval.get('source_sha256',{}).items())
            if source_valid and row['bytes']==approval['bytes'] and hashlib.sha256(get(row['address'],row['bytes'])).hexdigest()==approval['sha256']:
                reviewed_pools.append({**row,'review':approval['review'],'evidence':approval['evidence']})
            else:remaining.append(row)
    anonymous=remaining
    clean=not(unresolved or anonymous or storage_errors)
    report=build.BUILD/'strict-release-data-audit.json'
    report.write_text(json.dumps(dict(status='passed' if clean else 'blocked',rom_backed_objects=len(manifest['rows']),unreviewed_named_objects=len(unresolved),rows=unresolved,reviewed_code_objects=reviewed,anonymous_sections=anonymous,reviewed_compiler_tables=compiler_tables,reviewed_code_pools=reviewed_pools,storage_errors=storage_errors),indent=2)+'\n')
    if not clean:
        raise RuntimeError('Strict ROM-data release blocked: '+str(len(unresolved))+' unresolved initialized objects, '+str(len(anonymous))+' anonymous data sections, '+str(len(storage_errors))+' invalid ROM storage definitions. See '+str(report))
    print('PASS strict ROM-data release audit:',len(manifest['rows']),'ROM-backed definitions;',len(reviewed),'reviewed code objects; no unresolved original initialized data')
    return report

def main():
    rows=inventory();out=build.BUILD/'original-data-audit.json'
    by_owner=Counter()
    for r in rows:by_owner[Path(r['owner']).name]+=r['bytes']
    report=dict(status='NOT APPROVED FOR A ROM-ONLY DATA RELEASE',objects=len(rows),bytes=sum(r['bytes'] for r in rows),owners=dict(by_owner.most_common()),rows=rows,
                scope='Live initialized named objects only. Anonymous strings, local literals and compiler-generated tables require additional review. Engine instructions are outside the data audit.')
    out.write_text(json.dumps(report,indent=2)+'\n')
    print(report['status']);print(report['objects'],'original-engine objects;',report['bytes'],'initialized bytes')
    for owner,n in by_owner.most_common(12):print(owner,n)
    print(out)
if __name__=='__main__':main()
