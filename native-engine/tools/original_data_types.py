"""Private host audit: ARM DWARF layout of live original data objects."""
import concurrent.futures,json,re,subprocess,os
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build,build_probe

def prepare_objects():
    audit=json.loads((build.BUILD/'original-data-audit.json').read_text())
    sources={Path(r).name:r for r in json.loads((build.TARGET/'source-files.json').read_text())}
    names=set()
    for row in audit['rows']:
        owner=row['owner'];name=Path(owner.split('(')[-1].rstrip(')')).name
        if name.endswith('.c.o'):name=name[:-2]
        elif name.endswith('.o'):name=name[:-2]+'.c'
        if name in sources:names.add(name)
    out=build.BUILD/'original-data-types';out.mkdir(exist_ok=True)
    jobs=[]
    for name in sorted(names):
        src=build.source_path(Path(sources[name]));obj=out/(name+'.o');jobs.append((name,src,obj))
    def compile_job(job):
        name,src,obj=job
        result=subprocess.run([build.CC,*build_probe.engine_args(),'-g','-fno-eliminate-unused-debug-types','-c',src,'-o',obj],capture_output=True,text=True)
        if result.returncode:raise RuntimeError(name+'\n'+result.stderr)
        return name
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        done=list(pool.map(compile_job,jobs))
    print('ARM debug layouts:',len(done),'original units')
    return out

def scalar_fields(die,base=0,seen=None):
    seen=set() if seen is None else seen
    if die.offset in seen:raise ValueError('recursive value type')
    seen=seen|{die.offset};tag=die.tag;a=die.attributes
    if tag=='DW_TAG_typedef' and a.get('DW_AT_name') and a['DW_AT_name'].value in (b'Gfx',b'Gfx_le'):
        return [(base,4,'scalar'),(base+4,4,'scalar')]
    if tag in ('DW_TAG_const_type','DW_TAG_volatile_type','DW_TAG_typedef','DW_TAG_restrict_type'):
        return scalar_fields(die.get_DIE_from_attribute('DW_AT_type'),base,seen)
    if tag=='DW_TAG_pointer_type':return [(base,a.get('DW_AT_byte_size').value, 'pointer')]
    if tag in ('DW_TAG_base_type','DW_TAG_enumeration_type'):
        return [(base,a['DW_AT_byte_size'].value,'scalar')]
    if tag=='DW_TAG_array_type':
        element=die.get_DIE_from_attribute('DW_AT_type');size=type_size(element);count=1
        for child in die.iter_children():
            if child.tag!='DW_TAG_subrange_type':continue
            ca=child.attributes
            if 'DW_AT_count' in ca:n=ca['DW_AT_count'].value
            elif 'DW_AT_upper_bound' in ca:n=ca['DW_AT_upper_bound'].value+1
            else:raise ValueError('unknown array bound')
            if not isinstance(n,int) or n<0:raise ValueError("nonconstant array bound")
            count*=n
            if count>100000:raise ValueError("array outside audit budget")
        fields=[]
        for i in range(count):fields+=scalar_fields(element,base+i*size,seen)
        return fields
    if tag in ('DW_TAG_structure_type','DW_TAG_union_type'):
        if tag=='DW_TAG_union_type':
            choices=set()
            for child in die.iter_children():
                if child.tag!='DW_TAG_member':continue
                try: choices.add(tuple(scalar_fields(child.get_DIE_from_attribute('DW_AT_type'),base,seen)))
                except ValueError:
                    if not os.getenv('EOG_ROM_UNION_LAYOUT'):raise ValueError('union requires active-member review')
            if len(choices)!=1:
                if not os.getenv('EOG_ROM_UNION_LAYOUT') or not choices:raise ValueError('union requires active-member review')
                # A candidate memory view, never an approval: complete scalar
                # reconstruction and relocation validation are still required.
                return list(min(choices,key=lambda fields:(-sum(w for _,w,_ in fields),-len(fields),sum(k=='pointer' for _,_,k in fields))))
            return list(next(iter(choices)))
        fields=[]
        for child in die.iter_children():
            if child.tag!='DW_TAG_member':continue
            ca=child.attributes
            if 'DW_AT_bit_size' in ca:raise ValueError('bitfields require source-layout review')
            at=ca.get('DW_AT_data_member_location');at=at.value if at else 0
            if not isinstance(at,int):raise ValueError('nonconstant field location')
            fields+=scalar_fields(child.get_DIE_from_attribute('DW_AT_type'),base+at,seen)
        return fields
    raise ValueError(tag)

def type_size(die):
    a=die.attributes
    if 'DW_AT_byte_size' in a:return a['DW_AT_byte_size'].value
    if die.tag in ('DW_TAG_const_type','DW_TAG_volatile_type','DW_TAG_typedef','DW_TAG_restrict_type'):
        return type_size(die.get_DIE_from_attribute('DW_AT_type'))
    if die.tag=='DW_TAG_array_type':
        count=1
        for child in die.iter_children():
            if child.tag!='DW_TAG_subrange_type':continue
            ca=child.attributes
            if 'DW_AT_count' in ca:n=ca['DW_AT_count'].value
            elif 'DW_AT_upper_bound' in ca:n=ca['DW_AT_upper_bound'].value+1
            else:raise ValueError('unknown array bound')
            if not isinstance(n,int) or n<0:raise ValueError('nonconstant bound')
            count*=n
        return count*type_size(die.get_DIE_from_attribute('DW_AT_type'))
    raise ValueError('unknown type size '+die.tag)

def collect(out):
    result={}
    wanted={}
    for row in json.loads((build.BUILD/'original-data-audit.json').read_text())['rows']:
        owner=row['owner'].split('(')[-1].rstrip(')');name=Path(owner).name
        name=name[:-2] if name.endswith('.c.o') else name[:-2]+'.c'
        wanted.setdefault(name,set()).add(row['symbol'].split('.')[0])
    for path in sorted(out.glob('*.o')):
        variables={}
        with path.open('rb') as f:
            elf=ELFFile(f);dwarf=elf.get_dwarf_info()
            for cu in dwarf.iter_CUs():
                lines=dwarf.line_program_for_CU(cu)
                for die in cu.iter_DIEs():
                    if die.tag!='DW_TAG_variable':continue
                    specification=die.get_DIE_from_attribute('DW_AT_specification') if 'DW_AT_specification' in die.attributes else None
                    a={**(specification.attributes if specification else {}),**die.attributes}
                    if 'DW_AT_name' not in a or 'DW_AT_type' not in a:continue
                    name=a['DW_AT_name'].value.decode();row={}

                    if name not in wanted.get(path.name[:-2],set()):continue
                    try:
                        t=(die if 'DW_AT_type' in die.attributes else specification).get_DIE_from_attribute('DW_AT_type');row['size']=type_size(t);row['fields']=scalar_fields(t)
                    except ValueError as e:row['error']=str(e)
                    if 'DW_AT_decl_line' in a:row['line']=a['DW_AT_decl_line'].value
                    if 'DW_AT_decl_file' in a:
                        idx=a['DW_AT_decl_file'].value
                        try:row['file']=lines['file_entry'][idx-1].name.decode()
                        except (IndexError,AttributeError):pass
                    # Favor a concrete definition over an extern declaration.
                    if name not in variables or 'DW_AT_location' in die.attributes: variables[name]=row
        result[path.name[:-2]]=variables
    target=out/'layouts.json';target.write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':collect(prepare_objects())
