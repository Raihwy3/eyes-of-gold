"""Replace original title assets with empty storage; retain only ROM layout metadata."""
import re
import build

def generate():
    source=(build.UP/'assets/rarewarelogo.c').read_text()
    source=re.sub(r'//[^\n]*|/\*.*?\*/','',source,flags=re.S)
    arrays=[];offset=0
    for m in re.finditer(r'\b(u8|u32|Vtx|Gfx)\s+(\w+)\s*\[[^]]*\]\s*=\s*\{',source):
        kind,name=m.groups();depth=1;end=m.end()
        while depth:
            depth+=(source[end]=='{')-(source[end]=='}');end+=1
        body=source[m.end():end-1]
        if kind=='Gfx':size=len(re.findall(r'\bgs\w+\s*\(',body))*8
        elif kind=='Vtx':size=body.count('{')*16
        else:size=len(re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',body))*(4 if kind=='u32' else 1)
        assert size,(kind,name)
        # Symbol names include the retail segment offset, except the one text DL.
        encoded=re.search(r'(?:0x|D_0200|verts|rgba)([\dA-Fa-f]+)$',name)
        if encoded:assert int(encoded[1],16)==offset,(name,hex(offset))
        arrays.append((name,offset,size,{'u8':0,'u32':1,'Vtx':2,'Gfx':3}[kind]));offset+=size
    assert offset==0x67f0,(hex(offset),len(arrays))
    header=build.BUILD/'include/native_title_layout.h'
    text='/* ROM offsets and empty destination symbols only; no title pixels, mesh or commands. */\nextern "C" {\n'
    text+='\n'.join('extern unsigned char %s[%d];'%(n,size) for n,_,size,_ in arrays)+'\n}\n'
    text+='static const TitleRange title_ranges[]={'+','.join('{%du,%du,%du,%s}'%(o,size,k,n) for n,o,size,k in arrays)+'};\n'
    header.write_text(text)
    storage=build.BUILD/'include/native_title_storage.h'
    storage.write_text('/* Zero-filled storage. Initialized from validated ROM-derived cache at startup. */\n'+ '\n'.join('unsigned char %s[%d] __attribute__((aligned(8)));'%(n,size) for n,_,size,_ in arrays)+'\n')
    return arrays
if __name__=='__main__':generate()
