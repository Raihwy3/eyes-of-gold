"""ROM-backed storage for reviewed, pointer-free original data definitions.

Initializers are used only in unevaluated typeof operands to retain the exact
C types/array bounds. Assembly supplies empty writable storage. Payload bytes
are never emitted; the runtime loader reads authentic ROM spans.
"""
import hashlib,json,re,subprocess
from pathlib import Path
import build,build_probe
from native_fixture import fnv
PLAN=build.TARGET/'rom-data-layout.json'

def unit_name(owner):
    name=Path(owner.split('(')[-1].rstrip(')')).name
    return name[:-2] if name.endswith('.c.o') else name[:-2]+'.c'

def candidates():
    data=json.loads((build.BUILD/'original-data-matches.json').read_text())
    return [r for r in data['rows'] if r.get('bank') and unit_name(r['owner'])!='zlib.c' and '.' not in r['symbol']]

# Token offsets preserve full source text while ignoring quoted braces/semicolons.
TOKENS=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|\s+|.',re.S)
def declarations(source,all_globals=False):
    start=0;brace=paren=bracket=0;function=False;assignment=False
    for match in TOKENS.finditer(source):
        t=match[0]
        if t.isspace() or t.startswith(('"',"'")):continue
        if t=='(' :paren+=1
        elif t==')':paren-=1
        elif t=='[':bracket+=1
        elif t==']':bracket-=1
        elif t=='=' and brace==paren==bracket==0:assignment=True
        elif t=='{':
            if not brace and not paren and not bracket:function=not assignment and ')' in source[start:match.start()]
            brace+=1
        elif t=='}':
            brace-=1
            if brace==0 and function:start=match.end();function=False;assignment=False
        elif t==';' and brace==paren==bracket==0:
            if assignment or all_globals:yield start,match.end()
            start=match.end();assignment=False

def transform(source,rows,unit):
    wanted={r['symbol']:r for r in rows};done=[];edits=[];storage=[]
    weak_aliases=[]
    def weak(match):
        if match[2] not in wanted:return match[0]
        weak_aliases.append((match[1],match[2]));return ''
    source=re.sub(r'(?m)^\s*#pragma\s+weak\s+(\w+)\s*=\s*(\w+)[ \t]*$',weak,source)
    for start,end in declarations(source):
        statement=source[start:end];eq=statement.find('=');left=statement[:eq];rhs=statement[eq+1:].strip().removesuffix(';').strip()
        selected=[]
        for name,row in wanted.items():
            m=re.fullmatch(r'(.*?)\b'+re.escape(name)+r'\s*((?:\[[^]]*\]\s*)*)',left,flags=re.S)
            if m:selected.append((name,row,m))
        if len(selected)!=1:continue
        name,row,m=selected[0];prefix,arrays=m.groups()
        if ',' in prefix or '__attribute__' in left:continue
        directives='\n'.join(re.findall(r'(?m)^\s*#[^\n]*',prefix))
        prefix=re.sub(r'(?m)^\s*#[^\n]*','',prefix)
        static=bool(re.search(r'\bstatic\b',prefix));kind=re.sub(r'\b(?:static|extern)\b','',prefix).strip()
        if not kind or kind.startswith(('typedef','__asm__')):continue
        alias='eog_rom_'+hashlib.sha256(unit.encode()).hexdigest()[:8]+'_'+name if static else name
        initializer=rhs if rhs.startswith('{') else '{'+rhs+'}'
        replacement='\nextern __typeof__(('+kind+arrays+')'+initializer+') '+name+' __asm__("'+alias+'");\n'
        edits.append((start,end,replacement));done.append({**row,'unit':unit,'link_name':alias})
        storage.append('__asm__(".pushsection .bss.'+alias+',\\\"aw\\\",%nobits\\n.balign 8\\n.global '+alias+'\\n.type '+alias+',%object\\n.size '+alias+','+str(row['bytes'])+'\\n'+alias+':\\n.zero '+str(row['bytes'])+'\\n.popsection\\n");')
    for start,end,replacement in reversed(edits):source=source[:start]+replacement+source[end:]
    # Some original units forward-declare const arrays without extern.
    # Those are tentative definitions, so they must also refer to our storage.
    tentative=[]
    for start,end in declarations(source,all_globals=True):
        statement=source[start:end]
        if '=' in statement or '__asm__' in statement:continue
        for row in done:
            match=re.fullmatch(r'(.*?)\b'+re.escape(row['symbol'])+r'\s*((?:\[[^]]*\]\s*)*);',statement,flags=re.S)
            if not match:continue
            prefix,arrays=match.groups()
            if ',' in prefix or '(' in prefix or re.search(r'\bextern\b',prefix):continue
            prefix=re.sub(r'\bstatic\b','',prefix)
            tentative.append((start,end,'\nextern '+prefix.strip()+' '+row['symbol']+arrays+' __asm__("'+row['link_name']+'");\n'))
    for start,end,replacement in reversed(tentative):source=source[:start]+replacement+source[end:]
    aliases={r['symbol']:r for r in done}
    for alias,target in weak_aliases:
        assert target in aliases,'ROM-backed weak alias target was not transformed'
        row=aliases[target]
        assembly='.weak '+alias+'\n.set '+alias+','+row['link_name']+'\n.type '+alias+',%object\n.size '+alias+','+str(row['bytes'])+'\n'
        storage.append('__asm__('+json.dumps(assembly)+');')
    source+='\n/* Empty ROM-backed storage; no original initializer bytes. */\n'+'\n'.join(storage)+'\n'
    return source,done

def generate():
    import original_pointer_plan
    relocation=original_pointer_plan.generate()
    allowed_pointers={r['symbol'] for r in relocation['rows']}
    rows=[r for r in candidates() if not r['pointer_offsets'] or r['symbol'] in allowed_pointers];by_unit={}
    for r in rows:by_unit.setdefault(unit_name(r['owner']),[]).append(r)
    sources={Path(p).name:Path(p) for p in json.loads((build.TARGET/'source-files.json').read_text())}
    out=build.BUILD/'rom-data-proof';out.mkdir(exist_ok=True);accepted=[]
    for unit,items in sorted(by_unit.items()):
        rel=sources[unit];source=build.source_path(rel)
        args=[build.CC,*build_probe.engine_args()]
        result=subprocess.run([*args,'-E','-P',source],check=True,capture_output=True,text=True)
        text,done=transform(result.stdout,items,unit)
        rewritten=out/(unit+'.i');rewritten.write_text(text)
        obj=out/(unit+'.o')
        compiled=subprocess.run([*args,'-c',rewritten,'-o',obj],capture_output=True,text=True)
        if compiled.returncode:
            (out/(unit+'.error')).write_text(compiled.stderr);raise RuntimeError(unit+' rewrite failed; see '+str(out/(unit+'.error')))
        accepted+=done
    # Only structural addresses/layout and integrity hashes enter the manifest.
    clean=[]
    from elftools.elf.elffile import ELFFile
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        elf=ELFFile(f);sections=[x for x in elf.iter_sections() if x['sh_type']=='SHT_PROGBITS' and x['sh_flags']&2]
        for row in accepted:
            section=next(x for x in sections if x['sh_addr']<=row['address'] and row['address']+row['bytes']<=x['sh_addr']+x['sh_size'])
            at=row['address']-section['sh_addr'];normalized=bytearray(section.data()[at:at+row['bytes']])
            for pointer in row['pointer_offsets']:normalized[pointer:pointer+4]=b'\0'*4
            row['hash']=row.get('pre_patch_hash',fnv(normalized))
    for r in accepted:
        clean.append({key:r[key] for key in ('symbol','unit','link_name','bytes','bank','source_offset','fields','hash','source_bytes') if key in r})
    from native_fixture import canonical_rom
    import zlib
    cdata=zlib.decompress(canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')[0x21992:0x331e0],-15)
    PLAN.write_text(json.dumps(dict(version=1,rom_sha1='abe01e4aeb033b6c0836819f549c791b26cfde83',cdata_hash=fnv(cdata),rows=clean,pointers=relocation['pointers'],getters=relocation['getters']),indent=2)+'\n')
    print('ROM-backed storage proof:',len(accepted),'objects,',sum(r['bytes'] for r in accepted),'bytes; '+str(len(rows)-len(accepted))+' declarations require review')
    return accepted


def runtime_plan():
    from original_string_plan import runtime_rows
    plan=json.loads(PLAN.read_text())
    plan['rows']+=runtime_rows()
    return plan


def emit_runtime_header():
    if not PLAN.exists():return
    plan=runtime_plan();ops=[];records=[];lines=[]
    menu={'game_control_styles','game_options_entries','D_800409D8'}
    included=menu&{r['symbol'] for r in plan['rows']}
    assert not included or included==menu,'Native menu conversion must be atomic'
    if included:lines.append('#define NATIVE_ROM_CUSTOM_MENU_DATA 1')
    for row in plan['rows']:
        first=len(ops);fields=sorted([f if len(f)==4 else [*f,f[0]] for f in row['fields']],key=lambda x:(x[1],x[2],x[0]));i=0
        while i<len(fields):
            at,width,kind,source=fields[i]
            stride=fields[i+1][0]-at if i+1<len(fields) and fields[i+1][1:3]==fields[i][1:3] else 0
            source_stride=fields[i+1][3]-source if stride else 0
            j=i+1
            while stride and j<len(fields) and fields[j]==[at+(j-i)*stride,width,kind,source+(j-i)*source_stride]:j+=1
            ops.append((at,width,j-i,stride,{'scalar':0,'pointer':1,'image_nibbles':2,'native_save_options':3}[kind],source,source_stride));i=j
        label=row['link_name'];lines.append('extern unsigned char '+label+'[];')
        records.append('{'+label+','+','.join(str(v)+'u' for v in (row['bytes'],row.get('source_bytes',row['bytes']),int(row['bank']=='rom'),row['source_offset'],first,len(ops)-first,row['hash']))+'}')
    for getter_index,getter in enumerate(plan.get('getters',[])):
        assert getter['unit'] is None,'Private getter needs source-level export'
        name,ref=getter['name'],getter['getter']
        lines.append('extern const void *'+ref+'(void);')
        assembly='.pushsection .text.'+ref+',"ax",%progbits\n.arm\n.balign 4\n.global '+ref+'\n.type '+ref+',%function\n'+ref+':\nldr r0,1f\nbx lr\n.balign 4\n1: .word '+name+'\n.size '+ref+',.-'+ref+'\n.popsection\n'
        lines.append('#ifdef NATIVE_ROM_DATA_TEST\nextern const void *native_rom_test_symbol(unsigned);\nconst void *'+ref+'(void){return native_rom_test_symbol('+str(getter_index)+'u); }\n#else\n__asm__('+json.dumps(assembly)+');\n#endif')
    pointer_rows=[]
    kinds={'identity':0,'cart':1,'cdata_address':2,'symbol':3,'segment_offset':4}
    for pointer in plan.get('pointers',[]):
        pointer_rows.append('{'+str(pointer['key'])+'u,'+str(kinds[pointer['kind']])+','+pointer.get('getter','0')+','+str(pointer.get('addend',pointer.get('offset',0)))+'u}')
    lines.append('static const struct NativeRomPointer native_rom_pointers[]={'+','.join(pointer_rows)+'};')
    lines+=['static const struct NativeRomField native_rom_fields[]={'+','.join('{'+','.join(str(x)+'u' for x in op)+'}' for op in ops)+'};',
            'static const struct NativeRomObject native_rom_objects[]={'+','.join(records)+'};',
            '#define NATIVE_ROM_CDATA_HASH '+str(plan['cdata_hash'])+'u']
    (build.BUILD/'include/native_rom_data_layout.h').write_text('\n'.join(lines)+'\n')

def rewrite(rel,source,compiler_args):
    if not PLAN.exists():return source
    rows=[r for r in json.loads(PLAN.read_text())['rows'] if r['unit']==Path(rel).name]
    if not rows:return source
    folder=build.BUILD/'rom-data-overlay';folder.mkdir(exist_ok=True)
    result=subprocess.run([*compiler_args,'-E','-P',source],check=True,capture_output=True,text=True)
    text,done=transform(result.stdout,rows,Path(rel).name)
    if {r['symbol'] for r in done}!={r['symbol'] for r in rows}:raise RuntimeError('ROM-backed declaration rewrite incomplete: '+str(rel))
    target=folder/(str(rel).replace('/','__')+'.i');target.write_text(text);return target

if __name__=="__main__":generate();emit_runtime_header()
