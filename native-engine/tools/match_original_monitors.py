"""Resolve retail monitor bytecode and restore native pointers on 32-bit ARM.

The inherited 64-bit port replaced jump pointers with enum indices. ARM can use
real native pointers again, so the ROM supplies every opcode/operand/branch.
"""
import json,re,struct,zlib
from collections import Counter
from elftools.elf.elffile import ELFFile
import build
from native_fixture import canonical_rom
BASE=0x80020d90


def main():
    path=build.BUILD/'original-data-matches.json';report=json.loads(path.read_text());rows={r['symbol']:r for r in report['rows'] if r['owner'].endswith('(propobj.o)')}
    source=(build.UP/'src/game/propobj.c').read_text();header=(build.UP/'src/game/chrai.h').read_text()
    block=header[header.index('#define _PORT_MONANIM_LIST(X)'):header.index('#define _PORT_MONANIM_ENUM')];enums=re.findall(r'X\((\w+)\)',block)
    definitions=list(re.finditer(r'u32\s+((?:monAnim|monRand)\w+)\[\]\s*=\s*\{.*?\n\};',source,re.S));origins={};cursor=None;previous=0
    for definition in definitions:
        name=definition[1];prefix=source[previous:definition.start()];labels=re.findall(r'//\[(800[\da-fA-F]{5})',prefix)
        if labels:cursor=int(labels[-1],16)
        assert cursor is not None and name in rows, name+' missing authentic extent'
        origins[name]=cursor;cursor+=rows[name]['bytes'];previous=definition.end()
    native={}
    with (build.BUILD/'original-data-reference/eyes-of-gold-native-game.elf').open('rb') as f:
        e=ELFFile(f);sections=[s for s in e.iter_sections() if s['sh_type']=='SHT_PROGBITS' and s['sh_flags']&2]
        for name in origins:
            row=rows[name];s=next(s for s in sections if s['sh_addr']<=row['address'] and row['address']+row['bytes']<=s['sh_addr']+s['sh_size']);at=row['address']-s['sh_addr'];native[name]=s.data()[at:at+row['bytes']]
    cart=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64');data=zlib.decompress(cart[0x21992:0x331e0],-15)
    lengths=[1,3,3,3,3,3,3,2,2,2,3,1,1,3,2,2];count=0
    for name,address in origins.items():
        row=rows[name];offset=address-BASE;raw=data[offset:offset+row['bytes']];words=list(struct.unpack('>'+str(row['bytes']//4)+'I',raw));expected=list(struct.unpack('<'+str(row['bytes']//4)+'I',native[name]));fields=[[i,4,'scalar'] for i in range(0,row['bytes'],4)];pointers={};at=0
        while at<len(words):
            op=words[at];assert op<len(lengths) and at+lengths[op]<=len(words),(name,at,op)
            if op in (9,10):
                index=expected[at+1];assert index<len(enums),(name,index)
                target=enums[index];assert words[at+1]==origins[target],(name,target,hex(words[at+1]),hex(origins[target]))
                pointers[(at+1)*4]=target;fields[at+1][2]='pointer'
            for index in range(at,at+lengths[op]):
                if index*4 not in pointers:assert words[index]==expected[index],(name,index,words[index],expected[index])
            at+=lengths[op]
        row.update(bank='cdata',source_offset=offset,fields=fields,pointer_offsets=list(pointers),pointer_targets={str(k):v for k,v in pointers.items()},status='ROM span matched; relocation review required',evidence='retail monitor command decoder; exact scalar operands and verified native jump targets')
        row.pop('reason',None);count+=1
    report['counts']=dict(Counter(r['status'] for r in report['rows']));path.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS original monitor scripts:',count,'complete programs; scalar operands exact and retail jump addresses verified')
if __name__=='__main__':main()
