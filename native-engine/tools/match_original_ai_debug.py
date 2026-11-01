"""Load an original global AI list using the port's PRINT-record convention.

The PC engine's global-list parser expects PRINT to occupy one opcode byte.
The retail ROM also contains a NUL-terminated diagnostic after that opcode.
Only those diagnostics are omitted; every executable byte comes from the ROM.
"""
import difflib,json,zlib
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());row=next(r for r in report['rows'] if r['symbol']=='m_RunToBondPersistent')
    plan=json.loads((build.BUILD/'original-pointer-plan.json').read_text());names={g['getter']:g['name'] for g in plan['getters']}
    origins={p['key']-p.get('addend',0) for p in plan['pointers'] if p['kind']=='symbol' and names[p['getter']]==row['symbol']};assert len(origins)==1
    offset=origins.pop()-0x80020d90
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(rom[0x21992:0x331e0],-15)
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);s=next(s for s in e.iter_sections() if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);start=row['address']-s['sh_addr'];native=s.data()[start:start+row['bytes']]
    end=data.find(native[-16:],offset,offset+4096);assert end>=offset
    source=data[offset:end+16];fields=[];omitted=0
    for kind,lo,hi,first,last in difflib.SequenceMatcher(None,native,source,autojunk=False).get_opcodes():
        if kind=='equal':fields.extend([[i,1,'scalar',first+i-lo] for i in range(lo,hi)])
        else:
            assert kind=='insert' and first and source[first-1]==0xad and source[last-1]==0
            assert all(c in (9,10,13) or 32<=c<127 for c in source[first:last-1])
            omitted+=last-first
    rebuilt=bytes(source[f[3]] for f in fields)
    assert rebuilt==native and len(source)-omitted==len(native)
    row.update(bank='cdata',source_offset=offset,source_bytes=len(source),fields=fields,pointer_offsets=[],status='ROM span matched; relocation review required',evidence='whole original AI list; only PRINT diagnostic strings omitted to match existing global-list interpreter')
    row.pop('reason',None);report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS original AI debug adapter:',len(source),'source bytes ->',len(native),'runtime bytes; executable script identical')
if __name__=='__main__':main()
