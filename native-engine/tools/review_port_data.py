"""Record evidence-bound reviews for independently added code/runtime data.

This never waives an original game definition. It accepts only the classifier's
proof of absence from N64 source, our own native headers or compiler objects,
and pins each entry to its relocation-normalized bytes and upstream revision.
"""
import hashlib,json
from pathlib import Path
from elftools.elf.elffile import ELFFile
import build
from rom_data_plan import unit_name

def normalizer(elf):
    sections=[s for s in elf.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
    pointers=set()
    for s in elf.iter_sections():
        if s['sh_type']!='SHT_REL':continue
        target=elf.get_section(s['sh_info'])
        if target['sh_flags']&2 and not target['sh_flags']&4:
            pointers.update(r['r_offset'] for r in s.iter_relocations() if r['r_info_type'] in (2,38))
    def get(address,size):
        section=next(s for s in sections if s['sh_addr']<=address and address+size<=s['sh_addr']+s['sh_size'])
        offset=address-section['sh_addr'];data=bytearray(section.data()[offset:offset+size])
        for p in pointers:
            if address<=p and p+4<=address+size:data[p-address:p-address+4]=bytes(4)
        return bytes(data)
    return get

def main():
    source=json.loads((build.BUILD/'original-data-provenance.json').read_text());rows=[]
    allowed={'compiler-generated switch implementation','compiler-generated function-name diagnostic','native project implementation data','port/native implementation data'}
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        get=normalizer(ELFFile(f))
        for r in source['reviewed']:
            assert r['review'] in allowed
            rows.append(dict(unit=unit_name(r['owner']),symbol=r['symbol'],bytes=r['bytes'],sha256=hashlib.sha256(get(r['address'],r['bytes'])).hexdigest(),review=r['review'],evidence=r['evidence']))
    target=build.TARGET/'rom-data-code-review.json';target.write_text(json.dumps(dict(version=1,upstream=build.UPSTREAM_REV,rows=rows),indent=2)+'\n')
    print('Code-only provenance records:',len(rows))
if __name__=='__main__':main()
