"""Remove original ROM strings from compiler literal pools before linking.

Each complete matching string is replaced by zeros in its writable section.
Native references and suffix-sharing keep their exact offsets. Startup restores
those ranges from the verified ROM. Metadata contains no original text bytes.
"""
import concurrent.futures,hashlib,json,struct,subprocess,tempfile,zlib
from pathlib import Path
from collections import defaultdict
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom,fnv
PLAN=build.TARGET/'rom-string-layout.json'


def runtime_rows():
    if not PLAN.exists():return []
    return [row for group in json.loads(PLAN.read_text())['sections'] for row in group['rows']]


def generate(units=None):
    previous=json.loads(PLAN.read_text())["sections"] if units and PLAN.exists() else []
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');cdata=zlib.decompress(cart[0x21992:0x331e0],-15);groups=[];count=0;total=0
    import build_probe,rom_data_plan
    build.prepare()
    sources={Path(p).name:p for p in build.source_files() if p.suffix=='.c'}
    if units:
        assert set(units)<=set(sources), "Unknown string-plan unit"
        sources={name:rel for name,rel in sources.items() if name in units}
    folder=build.BUILD/'rom-string-input';folder.mkdir(exist_ok=True)
    def compile_input(item):
        unit,rel=item;source=build.source_path(rel);args=[build.CC,*build_probe.engine_args()]
        if str(rel)=='port/src/libultra.c':args.insert(1,'-I'+str(build.BUILD/'include/libultra-system'))
        source=rom_data_plan.rewrite(rel,source,args);obj=folder/(unit+'.o')
        result=subprocess.run([*args,'-c',source,'-o',obj],capture_output=True,text=True)
        if result.returncode:
            (folder/(unit+'.error')).write_text(result.stderr);raise RuntimeError('Original string input compile failed: '+unit)
        return unit,obj
    # Always inspect fresh pre-patch objects. Reusing a patched object would
    # hide precisely the original strings this inventory is meant to find.
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=dict(pool.map(compile_input,sorted(sources.items())))
    for unit,rel in sorted(sources.items()):
        obj=objects[unit]
        with obj.open('rb') as f:
            e=ELFFile(f)
            for section in e.iter_sections():
                if section['sh_type']!='SHT_PROGBITS':continue
                from original_pool_records import records
                blob=section.data();rows=records(unit,section.name,blob,cdata);at=0
                named_string=False
                if not section['sh_flags']&0x20 and section.name.startswith('.rodata.'):
                    base=section.name[8:].split('.')[0]
                    named_string=bool(__import__('re').search(r'\bchar\s+'+__import__('re').escape(base)+r'\s*\[[^]]*\]\s*=\s*"',(build.UP/rel).read_text()))
                if not section['sh_flags']&0x20 and not named_string:
                    if rows:
                        groups.append(dict(unit=unit,section=section.name,destination=rows[0]['pool_destination'],bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),rows=rows))
                        count+=len(rows);total+=sum(r['bytes'] for r in rows)
                    continue
                uid=hashlib.sha256((unit+':'+section.name).encode()).hexdigest()[:16];new='.data.eog_rom_strings_'+uid
                while at<len(blob):
                    end=blob.find(b'\0',at)
                    if end<0:break
                    value=blob[at:end+1]
                    if end>at:
                        source=cdata.find(value);bank='cdata'
                        if source<0:source=cart.find(value);bank='rom'
                        if source>=0:
                            alias='eog_rom_string_'+uid+'_'+format(at,'x')
                            rows.append(dict(unit=unit,symbol=alias,link_name=alias,bytes=len(value),bank=bank,source_offset=source,fields=[[i,1,'scalar'] for i in range(len(value))],hash=fnv(value),pool_section=section.name,pool_destination=new,pool_offset=at,storage='zeroed-string-pool'))
                            count+=1;total+=len(value)
                    at=end+1
                if rows:groups.append(dict(unit=unit,section=section.name,destination=new,bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest(),rows=rows))
    if units:groups=sorted([g for g in previous if g["unit"] not in units]+groups,key=lambda g:(g["unit"],g["section"]))
    PLAN.write_text(json.dumps(dict(version=1,upstream=build.UPSTREAM_REV,sections=groups),indent=2)+'\n')
    print('Original compiler-pool strings:',count,'strings;',total,'bytes in',len(groups),'sections')


def patch_object(unit,obj):
    if not PLAN.exists():return
    groups=[g for g in json.loads(PLAN.read_text())['sections'] if g['unit']==Path(unit).name]
    if not groups:return
    obj=Path(obj);sections={}
    with obj.open('rb') as f:
        e=ELFFile(f)
        for group in groups:
            section=e.get_section_by_name(group['section'])
            if section is None:raise RuntimeError('ROM string section missing: '+str(unit)+' '+group['section'])
            blob=bytearray(section.data())
            if hashlib.sha256(blob).hexdigest()!=group['sha256']:raise RuntimeError('ROM string compiler layout changed: '+str(unit)+' '+group['section'])
            for row in group['rows']:blob[row['pool_offset']:row['pool_offset']+row['bytes']]=bytes(row['bytes'])
            sections[group['section']]=blob
    with tempfile.TemporaryDirectory(dir=obj.parent) as name:
        folder=Path(name);args=[build.CC.with_name('arm-none-eabi-objcopy')]
        for index,group in enumerate(groups):
            data=folder/(str(index)+'.bin');data.write_bytes(sections[group['section']])
            args+=['--update-section',group['section']+'='+str(data),'--rename-section',group['section']+'='+group['destination']+',alloc,load,data,contents']
        first=folder/'sections.o';subprocess.run([*args,obj,first],check=True,capture_output=True)
        args=[build.CC.with_name('arm-none-eabi-objcopy')]
        for group in groups:
            for row in group['rows']:args+=['--add-symbol',row['link_name']+'='+group['destination']+':'+str(row['pool_offset'])+',global,object']
        target=folder/'symbols.o';subprocess.run([*args,first,target],check=True,capture_output=True)
        sizes={row['link_name']:row['bytes'] for group in groups for row in group['rows']};raw=bytearray(target.read_bytes())
        with target.open('rb') as f:
            e=ELFFile(f);assert e.elfclass==32 and e.little_endian
            table=e.get_section_by_name('.symtab')
            for index,symbol in enumerate(table.iter_symbols()):
                if symbol.name in sizes:struct.pack_into('<I',raw,table['sh_offset']+index*table['sh_entsize']+8,sizes.pop(symbol.name))
        assert not sizes,'Missing generated ROM string storage symbols'
        obj.write_bytes(raw)


def check_objects():
    plan=json.loads(PLAN.read_text());out=build.BUILD/'rom-string-proof';out.mkdir(exist_ok=True);done=set()
    for group in plan['sections']:
        unit=group['unit']
        if unit in done:continue
        source=build.BUILD/'rom-string-input'/(unit+'.o')
        target=out/(unit+'.o');target.write_bytes(source.read_bytes());patch_object(unit,target)
        with target.open('rb') as f:
            e=ELFFile(f);symbols={s.name:s for s in e.get_section_by_name('.symtab').iter_symbols()}
            for g in plan['sections']:
                if g['unit']!=unit:continue
                section=e.get_section_by_name(g['destination']);assert section['sh_flags']&1 and not section['sh_flags']&0x20
                for row in g['rows']:
                    symbol=symbols[row['link_name']];assert symbol['st_size']==row['bytes']
                    assert not any(section.data()[row['pool_offset']:row['pool_offset']+row['bytes']])
        done.add(unit)
    print('PASS original string object rewrite:',len(done),'units; exact aliases, zero original payload and writable nonmerged storage')

if __name__=='__main__':generate();check_objects()
