#!/usr/bin/env python3
"""Native-width original engine assets; never uses recreation meshes/scripts."""
import ast
import csv
import json
import re
import struct
import zlib
from pathlib import Path
from native_fixture import canonical_rom,fnv
import build


def original_helpers():
    source=build.UP/'tools_pc/d43_emit.py'
    tree=ast.parse(source.read_text())
    keep={'N','be16','bu16','be32o','be32r','build_nodes','gdl_of','gdl_end'}
    nodes=[n for n in tree.body if isinstance(n,(ast.FunctionDef,ast.ClassDef)) and n.name in keep]
    scope={'struct':struct,'MASK':0xffffff}
    exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),scope)
    return scope

# Native ARM32 records keep the retail pointer/GBI widths. Only scalar byte
# order changes. Derive traversal from the pinned original port's model walker.
MODEL_FIELDS={
 1:([4],[0,2,8,10,12]),2:([0,4,8,20,24],[12,14,16,18]),
 3:([0,4,8,20,24],[12,14,16,18]),4:([0,4,8,12],[16]),
 # BSP +0x20 is named "reserved", but selects the plane-normal axis (0..3)
 # in modelUpdateReorderRelations. It is not padding: leaving it big-endian
 # feeds 256/512/768 into a routine that only initializes normals for 0..3.
 8:([0,4,8],[12]),9:([0,4,8,12,16,20,24,28],[32,34]),
 10:([0,4,8,12,16,20,24],[]),12:([0,4,8,12,16,20,24,28],[32]),
 13:([0,4,8,12,16,20,24],[]),15:([0,4,8,12,16,20,24],[]),
 18:([0],[4]),21:([0,4,8,16],[12,14]),22:([0,4,8],[]),
 23:([],[0]),24:([0,4,8,16,20],[12,14,24,26])}


def convert_model(src,ns,nt,helpers,recipe=None):
    nodes,root=helpers['build_nodes'](src,len(src),ns,nt)
    assert nodes and root in nodes
    fields={};vertices={};collision={};pointusage={};lists=set();zero=[]
    def field(offset,width):
        assert offset>=0 and offset+width<=len(src),(offset,width,len(src))
        for at in range(offset,offset+width):
            assert at not in fields or fields[at]==(offset,width),(offset,width,fields[at])
            fields[at]=(offset,width)
    for i in range(ns):field(4*i,4)
    for i in range(nt):field(4*ns+12*i,4)
    u32=lambda off:struct.unpack_from('>I',src,off)[0]
    u16=lambda off:struct.unpack_from('>H',src,off)[0]
    ptr=lambda off:u32(off)&0xffffff
    for off,node in nodes.items():
        assert node.op in MODEL_FIELDS,node.op
        field(off,2)
        for at in (4,8,12,16,20):field(off+at,4)
        d=node.data
        words,halves=MODEL_FIELDS[node.op]
        for at in words:field(d+at,4)
        for at in halves:field(d+at,2)
        if node.op in (4,24):
            lists.update(p for p in (ptr(d),ptr(d+4)) if p)
        if node.op==4:
            vo,n=ptr(d+12),u16(d+16)
            if vo:
                if n:vertices[vo]=max(vertices.get(vo,0),n)
                else:zero.append(vo)
        elif node.op==24:
            vo,n=ptr(d+8),u16(d+12)
            if vo and n:vertices[vo]=max(vertices.get(vo,0),n)
            vo,n=ptr(d+16),u16(d+14)
            if vo and n:collision[vo]=max(collision.get(vo,0),n)
            vo,n=ptr(d+20),u16(d+12)
            if vo and n:pointusage[vo]=max(pointusage.get(vo,0),n)
        elif node.op==22:
            vo,n=ptr(d+4),u32(d)
            if vo and n:vertices[vo]=max(vertices.get(vo,0),4*n)
            if ptr(d+8):lists.add(ptr(d+8))
    boundaries=sorted(set(nodes)|{n.data for n in nodes.values()}|set(vertices)|set(collision)|lists|{len(src)})
    for vo in zero:
        end=next(o for o in boundaries if o>vo)
        assert (end-vo)%16==0
        vertices[vo]=(end-vo)//16
    for vo,n in vertices.items():
        for i in range(n):
            for j in range(0,12,2):field(vo+16*i+j,2)
    for vo,n in collision.items():
        for i in range(n):
            for j in (0,2,4,6,12,14):field(vo+16*i+j,2)
            field(vo+16*i+8,4)
    for vo,n in pointusage.items():
        for i in range(n):field(vo+2*i,2)
    commands=0
    for start in lists:
        end=helpers['gdl_end'](src,len(src),start)
        assert end>start and (end-start)%8==0
        for off in range(start,end,8):field(off,4);field(off+4,4);commands+=1
    out=bytearray(src)
    for offset,width in set(fields.values()):out[offset:offset+width]=src[offset:offset+width][::-1]
    if recipe is not None:recipe.extend(sorted(set(fields.values())))
    inverse=bytearray(out)
    for offset,width in set(fields.values()):inverse[offset:offset+width]=inverse[offset:offset+width][::-1]
    assert inverse==src
    return bytes(out),{'nodes':len(nodes),'commands':commands,'vertices':sum(vertices.values()),'fields':len(set(fields.values()))}


def model_counts():
    result={}
    for path in (build.UP/'assets').rglob('*'):
        if not path.name.lower().endswith('modelfileheader.inc.c'):continue
        for match in re.finditer(r'MODELFILEHEADER\(([^\n]+)\)',path.read_text()):
            a=[v.strip() for v in match[1].split(',')]
            try:result[a[0].lower()]=(int(a[4],0),int(a[8],0))
            except (ValueError,IndexError):pass
    return result


def model_sources(rom):
    counts=model_counts();helpers=original_helpers();results=[]
    for row in csv.reader((build.UP/'scripts/filelist.u.csv').open()):
        name=Path(row[2]).stem
        if not name or name[0] not in 'CGP' or not name.endswith('Z') or '_stan' in name:continue
        address,size=map(int,row[:2])
        if not size:continue
        raw=zlib.decompress(rom[address+2:address+size],-15)
        count=next((counts[n.lower()] for n in (name,name[1:],name[:-1],name[1:-1]) if n.lower() in counts),None)
        assert count is not None,name
        native,stats=convert_model(raw,*count,helpers)
        def packed_model(data):
            packed=zlib.compressobj(6,zlib.DEFLATED,-15)
            return b'\x11\x72'+packed.compress(data)+packed.flush()
        results.append((name,packed_model(native),len(native),stats))
        import weapon_skin_assets
        golden,gold_vertices=weapon_skin_assets.apply(name,raw,native,count,helpers)
        if gold_vertices:
            results.append(('@gold/'+name,packed_model(golden),len(golden),dict(gold_vertices=gold_vertices)))
            print('Gold silenced PP7:',name,gold_vertices,'shaded weapon vertices; original variant retained')
    return results



def convert_setup(src,recipe=None):
    """Native ARM32 stage setup; offsets, strides, strings and AI stay in place."""
    import sys
    sys.path.insert(0,str(build.UP/'tools_pc'))
    from d88_propdefs import PROPDEF_N64_WORDS
    fields={}
    def swap(at,width=4):
        assert at>=0 and at+width<=len(src)
        for i in range(at,at+width):
            assert i not in fields or fields[i]==(at,width)
            fields[i]=(at,width)
    u32=lambda at:struct.unpack_from('>I',src,at)[0]
    signed=lambda at:struct.unpack_from('>i',src,at)[0]
    header=struct.unpack_from('>10I',src)
    for at in range(0,40,4):swap(at)
    arrays=set();table_counts={}
    # (header index, serialized record stride, terminator offset, negative?)
    for index,stride,term,negative in ((0,16,0,True),(1,12,0,False),
            (4,8,0,False),(5,8,0,False),(6,44,36,False),
            (7,68,36,False),(8,4,0,False),(9,4,0,False)):
        start=header[index]
        if not start:continue
        at=start;count=0
        while True:
            value=signed(at+term) if negative else u32(at+term)
            ended=value<0 if negative else value==0
            if index==4:
                swap(at);swap(at+6,2)
            else:
                for off in range(0,stride,4):swap(at+off)
            if ended:break
            if index==0:arrays.add(u32(at+4))
            elif index==1:arrays.update((u32(at),u32(at+4)))
            elif index==4:arrays.add(u32(at))
            at+=stride;count+=1
            assert count<10000
        table_counts[index]=count
    for at in arrays:
        if not at:continue
        while True:
            swap(at)
            if signed(at)<0:break
            at+=4
    intro_sizes={0:12,1:16,2:16,3:32,4:8,5:8,6:40,7:12,8:8,9:4}
    at=header[2]
    while at:
        kind=u32(at);size=intro_sizes[kind]
        for offset in range(0,size,4):
            if kind==6 and offset==28:swap(at+offset,2);swap(at+offset+2,2)
            else:swap(at+offset)
        at+=size
        if kind==9:break
    at=header[3];counts={}
    object_types={1,3,4,5,6,7,8,10,11,12,13,17,20,21,36,39,40,42,43,45,47}
    supported=object_types|{2,9,14,18,19,22,23,24,25,26,27,28,30,32,33,34,35,37,38,44,46,48}
    while at:
        kind=src[at+3];assert kind in supported,kind
        words=PROPDEF_N64_WORDS[kind]
        counts[kind]=counts.get(kind,0)+1
        swap(at,2) # extrascale; state/type remain byte ordered
        for i in range(1,words):
            off=at+4*i
            if kind in object_types and i in (30,31):continue # RGBA bytes
            if kind==11 and i>=32+29*4:continue # four image-number bytes
            if (kind in object_types and i==1) or (kind==9 and i<=5) or (kind==22 and i==1) or (kind==1 and i==38) or (kind in (39,40) and i==33) or (kind==20 and i>=32):
                swap(off,2);swap(off+2,2)
            elif kind==18 and i==2:swap(off,2) # halfword + two byte-sized attributes
            elif (kind==8 and i==32) or (kind==1 and i==47):swap(off+2,2)
            elif kind==1 and i==49:swap(off,2)
            else:swap(off)
        at+=4*words
        if kind==48:break
    assert at==header[2],(at,header[2])
    prop_end=at
    out=bytearray(src)
    for at,width in set(fields.values()):out[at:at+width]=src[at:at+width][::-1]
    if recipe is not None:recipe.extend(sorted(set(fields.values())))
    inverse=bytearray(out)
    for at,width in set(fields.values()):inverse[at:at+width]=inverse[at:at+width][::-1]
    assert inverse==src
    # AI-list bytecode must not be treated as native scalar words.
    return bytes(out),{'prop_types':counts,'tables':table_counts,'props_end':prop_end,'fields':len(set(fields.values()))}


def convert_bg(src,recipe=None):
    out=bytearray(src);fields=set()
    h=struct.unpack_from('>5I',src);rooms=h[1]&0xffffff;portals=h[2]&0xffffff;env=h[3]&0xffffff
    end=env if rooms<env<portals else portals
    def swap(at):
        assert 0<=at<=len(src)-4
        fields.add(at);out[at:at+4]=src[at:at+4][::-1]
    for at in range(0,20,4):swap(at)
    for at in range(rooms,end,4):swap(at)
    if env:
        for at in range(env,portals,8):swap(at+4)
    targets=set()
    if env:
        for at in range(env,portals,8):
            if src[at]==100 and struct.unpack_from('>I',src,at+4)[0]:targets.add(struct.unpack_from('>I',src,at+4)[0]&0xffffff)
    at=portals
    while struct.unpack_from('>I',src,at)[0]:
        targets.add(struct.unpack_from('>I',src,at)[0]&0xffffff);swap(at);at+=8
    for target in targets:
        count=src[target];assert 0<count<=20
        for at in range(target+4,target+4+12*count,4):swap(at)
    # Compressed vertices/GDL remain BE: original bgLoadRoomModelData converts
    # them after decompression, with sizeof(Gfx) selecting the native stride.
    if recipe is not None:recipe.extend((at,4) for at in sorted(fields))
    inverse=bytearray(out)
    for at in fields:inverse[at:at+4]=inverse[at:at+4][::-1]
    assert inverse==src
    return bytes(out)


def convert_stan(src,recipe=None):
    out=bytearray(src);first=struct.unpack_from('>I',src,4)[0]
    for at in range(0,first,4):out[at:at+4]=src[at:at+4][::-1]
    at=first;sizes=[32,32,32,32,40,48,56,64,72,80,88]
    while any(src[at:at+4]):
        count=src[at+6]>>4;assert 3<=count<=10
        out[at:at+2]=src[at:at+2][::-1]
        for off in range(at+4,at+8+8*count,2):out[off:off+2]=src[off:off+2][::-1]
        at+=sizes[count]
    if recipe is not None:
        recipe.extend((at,4) for at in range(0,first,4))
        at=first
        while any(src[at:at+4]):
            count=src[at+6]>>4
            recipe.append((at,2));recipe.extend((off,2) for off in range(at+4,at+8+8*count,2))
            at+=sizes[count]
    return bytes(out)


def create_game_assets(rom):
    rows=model_sources(rom)
    table={Path(r[2]).stem:tuple(map(int,r[:2])) for r in csv.reader((build.UP/'scripts/filelist.u.csv').open())}
    stats={}
    import stage_assets
    for name,kind,compressed in stage_assets.conversions():
        address,size=table[name]
        raw=zlib.decompress(rom[address+2:address+size],-15) if compressed else bytes(rom[address:address+size])
        native=kind(raw)
        if isinstance(native,tuple):native,stats[name]=native
        if compressed:
            pack=zlib.compressobj(6,zlib.DEFLATED,-15);blob=b'\x11\x72'+pack.compress(native)+pack.flush()
        else:blob=native
        rows.append(("bg/"+name+".seg" if not compressed else name,blob,len(native),{}))
    directory=bytearray();payload=bytearray();offset=24+len(rows)*80
    for name,blob,raw_size,meta in rows:
        assert len(name)<64
        directory+=struct.pack('<64s4I',name.encode(),offset,len(blob),raw_size,fnv(blob))
        payload+=blob;offset+=len(blob)
    header=struct.pack('<8s4I',b'EOGGAME1',1,len(rows),fnv(directory+payload),fnv(rom))
    path=build.BUILD/'native-game.dat';path.write_bytes(header+directory+payload)
    (build.BUILD/'native-cart.bin').write_bytes(rom)
    return rows,stats

def main():
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    build.BUILD.mkdir(exist_ok=True)
    rows,setup_stats=create_game_assets(rom)
    import stage_assets
    models=len(rows)-sum(1 for _ in stage_assets.conversions())
    (build.BUILD/'native-model-conversion.json').write_text(json.dumps({
        'models':models,'setup':setup_stats,'unpacked_bytes':sum(r[2] for r in rows),
        'nodes':sum(r[3].get('nodes',0) for r in rows),'files':{r[0]:r[3] for r in rows}},indent=2)+'\n')
    print('Native ARM32 assets:',models,'models + Dam/Facility/Runway/Surface/Bunker/Silo/Frigate/Surface 2/Bunker 2/Statue/Archives/Streets/Depot/Train/Jungle/Control/Caverns/Cradle/Aztec/Egyptian BG/STAN/setup converted; model/setup inverse checks pass')

if __name__=='__main__':main()
