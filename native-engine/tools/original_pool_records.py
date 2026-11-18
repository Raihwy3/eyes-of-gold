"""Typed original local-initializer records emitted in anonymous .rodata.

These are complete original sound-ID lists, save defaults and hat construction
records. The private compiler bytes must reconstruct exactly from ROM fields.
No payload bytes enter the returned metadata.
"""
import os,hashlib
from elftools.elf.elffile import ELFFile
import build
from original_data_types import scalar_fields,type_size
from native_fixture import fnv


def records(unit,section,blob,cdata):
    if section!='.rodata' or unit not in ('chraction.c','file2.c','propobj.c'):return []
    if unit=='chraction.c':
        assert len(blob)==74
        specs=[(0,50,[[i,2,'scalar'] for i in range(0,50,2)]),(52,22,[[i,2,'scalar'] for i in range(0,22,2)])]
    elif unit=='file2.c':
        assert len(blob)==128
        save=[[0,4,'scalar'],[4,4,'scalar']]+[[i,1,'scalar'] for i in range(8,12)]+[[12,2,'native_save_options']]+[[i,1,'scalar'] for i in range(14,96)]
        small=[[0,4,'scalar'],[4,4,'scalar']]+[[i,1,'scalar'] for i in range(8,32)]
        specs=[(0,96,save),(96,32,small)]
    else:
        assert len(blob)==128
        with (build.BUILD/'original-data-types/propobj.c.o').open('rb') as f:
            dwarf=ELFFile(f).get_dwarf_info();die=next(d for cu in dwarf.iter_CUs() for d in cu.iter_DIEs() if d.tag=='DW_TAG_typedef' and d.attributes.get('DW_AT_name') and d.attributes['DW_AT_name'].value==b'HatRecord')
            previous=os.environ.get('EOG_ROM_UNION_LAYOUT');os.environ['EOG_ROM_UNION_LAYOUT']='1'
            try:fields=scalar_fields(die);assert type_size(die)==len(blob)
            finally:
                if previous is None:os.environ.pop('EOG_ROM_UNION_LAYOUT',None)
                else:os.environ['EOG_ROM_UNION_LAYOUT']=previous
        specs=[(0,len(blob),fields)]
    rows=[];uid=hashlib.sha256((unit+':'+section).encode()).hexdigest()[:16]
    for offset,size,fields in specs:
        native=blob[offset:offset+size];expected=bytearray(size)
        for at,width,kind in fields:
            assert kind!='pointer' or not any(native[at:at+width]),'non-null constructor pointer requires a native address binding'
            if kind=='native_save_options':expected[at:at+width]=(int.from_bytes(native[at:at+width],'little')|16).to_bytes(width,'big')
            else:expected[at:at+width]=native[at:at+width][::-1]
        source=cdata.find(expected)
        if source<0:raise RuntimeError('Complete original compiler record not found: '+unit+' offset '+str(offset))
        rebuilt=bytearray(size)
        for at,width,kind in fields:
            value=cdata[source+at:source+at+width]
            rebuilt[at:at+width]=(int.from_bytes(value,'big')&~16).to_bytes(width,'little') if kind=='native_save_options' else value[::-1]
        if rebuilt!=native:raise RuntimeError('Original compiler record reconstruction failed: '+unit)
        alias='eog_rom_record_'+uid+'_'+format(offset,'x')
        rows.append(dict(unit=unit,symbol=alias,link_name=alias,bytes=size,bank='cdata',source_offset=source,fields=fields,hash=fnv(native),pool_section=section,pool_destination='.data.eog_rom_strings_'+uid,pool_offset=offset,storage='zeroed-compiler-pool'))
    return rows
