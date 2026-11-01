"""ROM offsets and record-layout metadata only, without geometry/image payloads."""
import csv
from pathlib import Path
import build,native_game_assets as assets,stage_assets
from native_fixture import canonical_rom

def generate():
    counts=assets.model_counts();models=[]
    rows=list(csv.reader((build.UP/'scripts/filelist.u.csv').open()))
    for row in rows:
        name=Path(row[2]).stem;offset,size=map(int,row[:2])
        if not size or name[:1] not in ('C','P') or not name.endswith('Z') or '_stan' in name:continue
        count=next((counts[n.lower()] for n in (name,name[1:],name[:-1],name[1:-1]) if n.lower() in counts),None)
        if count is None:continue
        models.append((offset,size,4*count[0]+12*count[1],int(name.startswith('P')),1 if name=='PmiltruckZ' else 2 if name=='Pboxes4x4Z' else 0))
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    backgrounds=list(stage_assets.backgrounds(rom))
    menu=next(r for r in rows if 'usedby7F008DE4.bin' in r[2])
    text='/* Structural offsets only; every mesh/command/image is read from ROM. */\n'
    text+='static const ModelLayout model_layouts[]={'+','.join('{'+','.join(str(v)+'u' for v in r)+'}' for r in models)+'};\n'
    text+='static const BgLayout bg_layouts[]={'+','.join('{%su,%su,%su}'%(offset,size,stop) for _,offset,size,_,_,stop in backgrounds)+'};\n'
    text+='static constexpr unsigned max_room_records=%su,menu_offset=%s,menu_bytes=%s;\n'%(sum(bg[-1]-1 for bg in backgrounds),*menu[:2])
    target=build.BUILD/'include/native_derived_layout.h';target.write_text(text);return target
if __name__=='__main__':generate()
