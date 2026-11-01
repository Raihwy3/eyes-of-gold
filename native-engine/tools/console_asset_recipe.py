"""Generate conversion instructions (offsets/strides, never original payload bytes).

The console reads and validates the user's ROM before applying these instructions.
Recipes share the desktop converter's checked field traversal.
"""
import csv,struct,zlib,itertools
from pathlib import Path
import build,native_game_assets as game,weapon_skin_assets as skin,stage_assets
from native_fixture import canonical_rom,fnv

def generate():
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    counts=game.model_counts();helpers=game.original_helpers();rows=[];ops=[]
    def add(name,offset,size,compressed,raw,native,fields,gold=False):
        start=len(ops);i=0;runs=[]
        fields=sorted(fields)
        while i<len(fields):
            at,width=fields[i];stride=fields[i+1][0]-at if i+1<len(fields) and fields[i+1][1]==width else 0
            j=i+1
            while stride and j<len(fields) and fields[j]==(at+(j-i)*stride,width):j+=1
            runs.append((at,j-i,stride,width));i=j
        # Swaps occupy disjoint byte ranges. Group identical runs into repeated
        # blocks, e.g. six halfword swaps in each 16-byte vertex.
        runs.sort(key=lambda r:(r[1:],r[0]))
        i=0
        while i<len(runs):
            at,count,stride,width=runs[i]
            pitch=runs[i+1][0]-at if i+1<len(runs) and runs[i+1][1:]==runs[i][1:] else 0
            j=i+1
            while pitch and j<len(runs) and runs[j]==(at+(j-i)*pitch,count,stride,width):j+=1
            ops.append((at,count,stride,width,j-i,pitch));i=j
        rows.append((name,offset,size,len(raw),fnv(native),start,len(ops)-start,int(compressed),int(gold)))
    for row in csv.reader((build.UP/'scripts/filelist.u.csv').open()):
        name=Path(row[2]).stem;offset,size=map(int,row[:2])
        if not size:continue
        if name[0:1] in ('C','G','P') and name.endswith('Z') and '_stan' not in name:
            raw=zlib.decompress(rom[offset+2:offset+size],-15)
            count=next(counts[n.lower()] for n in (name,name[1:],name[:-1],name[1:-1]) if n.lower() in counts)
            fields=[];native,_=game.convert_model(raw,*count,helpers,recipe=fields)
            add(name,offset,size,True,raw,native,fields)
            golden,n=skin.apply(name,raw,native,count,helpers)
            if n:
                # Vertices are not necessarily aligned relative to file start.
                changed=[at for at,(a,b) in enumerate(zip(native,golden)) if a!=b]
                goldfields=fields+[(at-1,0) for at in changed if native[at]!=golden[at] and native[at-1]==golden[at-1] and native[at+1]!=golden[at+1]]
                test=bytearray(raw)
                for at,width in sorted(goldfields):
                    if width:test[at:at+width]=test[at:at+width][::-1]
                    else:
                        value=test[at]
                        test[at:at+3]=bytes((value,(value*233+127)//255,(value*119+127)//255))
                assert test==golden,name
                add('@gold/'+name,offset,size,True,raw,golden,goldfields,True)
    files={Path(r[2]).stem:r for r in csv.reader((build.UP/'scripts/filelist.u.csv').open())}
    for name,convert,compressed in stage_assets.conversions():
        offset,size=map(int,files[name][:2]);raw=zlib.decompress(rom[offset+2:offset+size],-15) if compressed else rom[offset:offset+size]
        fields=[];native=convert(raw,recipe=fields)
        if isinstance(native,tuple):native=native[0]
        add('bg/'+name+'.seg' if not compressed else name,offset,size,compressed,raw,native,fields)
    text='/* Generated structural conversion instructions; no ROM payload. */\n'
    text+='static const struct ConvertOp convert_ops[]={\n'+',\n'.join('{%du,%du,%du,%du,%du,%du}'%op for op in ops)+'\n};\n'
    text+='static const struct ConvertRow convert_rows[]={\n'+',\n'.join('{"%s",%s}'%(r[0],','.join(str(v)+'u' for v in r[1:])) for r in rows)+'\n};\n'
    text+='#define CONVERT_ROWS %du\n#define CONVERT_CART_HASH %du\n'%(len(rows),fnv(rom))
    target=build.BUILD/'include/native_convert_recipe.h';target.write_text(text)
    print('Console conversion recipe:',len(rows),'files,',len(ops),'structural runs; largest raw file',max(r[3] for r in rows))
    return target
if __name__=='__main__':generate()
