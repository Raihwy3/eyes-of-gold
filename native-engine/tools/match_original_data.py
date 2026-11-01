"""Find authentic ROM table spans using ARM type layouts, without payload recipes.

Only complete typed scalar fields are compared. Pointer and padding bytes are
not guessed. Ambiguous/unsupported objects stay unresolved for release review.
"""
import json,re,zlib
from pathlib import Path
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom
BASE=0x80020d90

def unit_name(owner):
    name=Path(owner.split('(')[-1].rstrip(')')).name
    return name[:-2] if name.endswith('.c.o') else name[:-2]+'.c'

def main():
    audit=json.loads((build.BUILD/'original-data-audit.json').read_text())
    previous_path=build.BUILD/'original-data-matches.json'
    previous={(r['owner'],r['symbol']):r for r in json.loads(previous_path.read_text())['rows'] if r.get('bank')} if previous_path.exists() else {}
    types=json.loads((build.BUILD/'original-data-types/layouts.json').read_text())
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');cdata=zlib.decompress(rom[0x21992:0x331e0],-15)
    sources={};results=[]
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as stream:
        elf=ELFFile(stream);absolute=set()
        for section in elf.iter_sections():
            if section['sh_type']!='SHT_REL':continue
            target=elf.get_section(section['sh_info'])
            if not target['sh_flags']&2 or target['sh_flags']&4:continue
            for relocation in section.iter_relocations():
                if relocation['r_info_type'] in (2,38):absolute.add(relocation['r_offset'])
        sections=[s for s in elf.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for item in audit['rows']:
            row=dict(item);name=row['symbol'].split('.')[0];layout=types.get(unit_name(row['owner']),{}).get(name)
            prior=previous.get((row['owner'],row['symbol']),{})
            if 'pointer_targets' in prior or 'pre_patch_hash' in prior or prior.get('source_bytes',prior.get('bytes'))!=prior.get('bytes'):
                results.append(prior);continue
            sec=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size'])
            off=row['address']-sec['sh_addr'];native=sec.data()[off:off+row['bytes']]
            if not any(native):row['status']='zero state; no nonzero payload';results.append(row);continue
            if not layout:row['reason']='missing type';results.append(row);continue
            if layout.get('error'):row['reason']=layout['error'];results.append(row);continue
            if layout['size']!=len(native):row['reason']='native type/instance size mismatch';results.append(row);continue
            expected=bytearray(len(native));mask=bytearray(len(native));pointers=[]
            updated=[]
            for at,size,kind in layout['fields']:
                if size==4 and row['address']+at in absolute:kind='pointer'
                updated.append([at,size,kind])
                if kind=='pointer':pointers.append(at);continue
                if size not in (1,2,4,8) or at+size>len(native):raise ValueError((name,at,size))
                expected[at:at+size]=native[at:at+size][::-1];mask[at:at+size]=b'\1'*size
            layout={**layout,'fields':updated}
            if not any(mask) and not re.search(r'(?:D_|asc_D_)(800[\da-fA-F]{5})',name):
                # An adjacent address comment cannot identify a pointer-only
                # definition: scalar validation would accept any bytes. These
                # roots require complete pointer/string graph validation.
                row['reason']='pointer-only table needs native/original graph validation'
                results.append(previous.get((row['owner'],row['symbol']),row));continue
            def valid(data,at):return at>=0 and at+len(native)<=len(data) and all(not m or data[at+i]==expected[i] for i,m in enumerate(mask))
            candidates=[]
            # A labeled original address is stronger than a pattern search.
            label=re.search(r'(?:D_|asc_D_|_)(800[\da-fA-F]{5}|7F[\da-fA-F]{6})',name)
            addresses=[int(label[1],16)] if label else []
            file=layout.get('file');line=layout.get('line',0)
            if file:
                if file not in sources:sources[file]=list(build.UP.rglob(file))
                for source in sources[file]:
                    lines=source.read_text(errors='replace').splitlines()
                    # Overlay insertions shift DWARF line numbers relative to
                    # pinned source. Also inspect the original declaration's
                    # immediately preceding address comment.
                    declaration=re.compile(r'^[ \t]*(?:[A-Za-z_]\w*[ \t*]+)+'+re.escape(name)+r'\s*(?:\[[^]]*\]\s*)*=')
                    for index,text in enumerate(lines):
                        if declaration.match(text):
                            context='\n'.join(lines[max(0,index-2):index])
                            comment=re.search(r'(?:D:|D_|//\s*|//\[|/\*\s*D:)(800[\da-fA-F]{5})[^\n]*\n?\s*$',context)
                            if comment:addresses.append(int(comment[1],16))
                    if line and line<=len(lines) and name in '\n'.join(lines[max(0,line-2):line+3]):
                        prefix='\n'.join(lines[max(0,line-30):line])
                        matches=re.findall(r'(?:D:|D_|//\s*)(800[\da-fA-F]{5})',prefix)
                        if matches:addresses.append(int(matches[-1],16))
            for address in addresses:
                if BASE<=address<BASE+len(cdata) and valid(cdata,address-BASE):candidates.append(('cdata',address-BASE,'labeled address'))
                if 0x7f000000<=address<0x7f0e2d50 and valid(rom,address-0x7f000000+0x34b30):candidates.append(('rom',address-0x7f000000+0x34b30,'labeled address'))
            if not candidates:
                runs=[];i=0
                while i<len(mask):
                    if not mask[i]:i+=1;continue
                    j=i+1
                    while j<len(mask) and mask[j]:j+=1
                    runs.append((i,j));i=j
                runs.sort(key=lambda r:(len(set(expected[r[0]:r[1]])),r[1]-r[0]),reverse=True)
                if runs and runs[0][1]-runs[0][0]>=4:
                    start,end=runs[0];needle=bytes(expected[start:end])
                    for bank,data in [('cdata',cdata),('rom',rom)]:
                        position=0;hits=[];scanned=0
                        while (position:=data.find(needle,position))>=0 and scanned<4096:
                            candidate=position-start;position+=1;scanned+=1
                            if valid(data,candidate):hits.append(candidate)
                            if len(hits)>16:break
                        if len(hits)==1 and scanned<4096:candidates.append((bank,hits[0],'unique typed table'))
                        elif len(hits)>1 and scanned<4096 and not pointers and len(native)>=8:
                            # Identical complete scalar tables may occur more
                            # than once (e.g. default structs/lighting). A
                            # canonical whole copy is still authentic ROM data;
                            # never construct a table from isolated byte hits.
                            candidates.append((bank,hits[0],'canonical complete duplicate scalar table'))
                        if candidates:break
            unique={(bank,at) for bank,at,_ in candidates}
            if len(unique)==1:
                bank,at=next(iter(unique));row.update(status='ROM span matched; relocation review required',bank=bank,source_offset=at,pointer_offsets=pointers,fields=layout['fields'],evidence=candidates[0][2])
            else:row['reason']='ambiguous or absent ROM span';row['candidate_count']=len(unique)
            results.append(row)
    results=[previous.get((r['owner'],r['symbol']),r) if not r.get('bank') else r for r in results]
    counts=Counter(r['status'] for r in results);report=dict(status='RELEASE BLOCKED until all original payloads are ROM-loaded and verified',counts=dict(counts),rows=results)
    target=build.BUILD/'original-data-matches.json';target.write_text(json.dumps(report,indent=2)+'\n')
    print(dict(counts));print(Counter(r.get('reason') for r in results if r['status']=='unresolved'));print(target)
if __name__=='__main__':main()
