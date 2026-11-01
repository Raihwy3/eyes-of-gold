"""Original compressed Dam room/image fixture; no triangle/mesh reconstruction."""
import csv
import json
import struct
import zlib
from native_fixture import canonical_rom, fnv

HEADER_WORDS = 24

def create(rom_path, upstream, path, room=135):
    rom=canonical_rom(rom_path)
    with (upstream/'scripts/filelist.u.csv').open() as f:
        row=next(r for r in csv.reader(f) if r[2].endswith('/bg_dam_all_p.bin'))
    bg_offset,bg_size=map(int,row[:2]);bg=bytes(rom[bg_offset:bg_offset+bg_size])
    table=struct.unpack_from('>I',bg,4)[0]&0xffffff
    entry=table+room*24
    pointers=struct.unpack_from('>III',bg,entry)
    position=struct.unpack_from('>fff',bg,entry+12)
    blobs=[];compressed=[];ops={};textures=set();triangles=[]
    for index,address in enumerate(pointers):
        if not address:blobs.append(b'');compressed.append(0);continue
        assert address>>24==15
        offset=address&0xffffff;assert bg[offset:offset+2]==b'\x11\x72'
        dec=zlib.decompressobj(-15);raw=dec.decompress(bg[offset+2:]);assert dec.eof
        length=len(bg)-offset-len(dec.unused_data)
        assert length>2 and len(raw)<=65536
        blobs.append(raw);compressed.append(length)
        if index:
            n=0;cache=[False]*16;ended=False
            for at in range(0,len(raw)-7,8):
                a,b=struct.unpack_from('>II',raw,at);op=a>>24;ops[hex(op)]=ops.get(hex(op),0)+1
                if op==0xb8:ended=True;break
                if op==0xc0:
                    assert a&7 in (0,2,3,4);textures.add(b&4095)
                if op==4:
                    count=(a&65535)//16;dst=(a>>16)&15;off=b&0xffffff
                    assert b>>24==14 and dst+count<=16 and off+count*16<=len(blobs[0])
                    cache[dst:dst+count]=[True]*count
                if op in (0xb1,0xbf):
                    ts=[((b>>(8*i))&15,(b>>(8*i+4))&15,(a>>(4*i))&15) for i in range(4)] if op==0xb1 else [tuple(((b>>s)&255)//10 for s in (16,8,0))]
                    for t in ts:
                        if len(set(t))<3:continue
                        assert max(t)<16 and all(cache[v] for v in t);n+=1
            assert ended
            triangles.append(n)
    while len(triangles)<2:triangles.append(0)
    image_start=0x8f7df0;image_end=0xc00000;images=bytes(rom[image_start:image_end])
    fields=[1,room,0x10000000+bg_offset,len(bg),0x10000000+image_start,len(images),
            *map(len,blobs),*map(fnv,blobs),fnv(bg),fnv(images),*triangles,
            *compressed,table,entry,max(textures),len(textures),0]
    assert len(fields)==HEADER_WORDS
    path.write_bytes(struct.pack('<8s24I',b'EOGROOM1',*fields)+bg+images)
    manifest={'room':room,'position':position,'vertex_bytes':len(blobs[0]),'display_list_bytes':[len(b) for b in blobs[1:]],
              'triangles':triangles,'texture_ids':sorted(textures),'original_opcodes':ops,
              'compressed_bytes':compressed,'bg_bytes':len(bg),'image_segment_bytes':len(images),
              'policy':'Original big-endian compressed BG and texture segment. Runtime uses original decompressor/texture loader. No mesh or recreated scene assets.'}
    path.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def create_world(rom_path, upstream, path):
    """Metadata for original BG streams and unchanged compressed collision file."""
    rom=canonical_rom(rom_path)
    rows=list(csv.reader((upstream/'scripts/filelist.u.csv').open()))
    row=next(r for r in rows if r[2].endswith('/bg_dam_all_p.bin'))
    start,size=map(int,row[:2]);bg=rom[start:start+size]
    records=[]
    for room in range(1,137):
        entry=20+room*24
        pointers=struct.unpack_from('>III',bg,entry)
        fields=[room]
        for pointer in pointers:
            if not pointer: fields += [0,0,0,0];continue
            offset=pointer&0xffffff
            dec=zlib.decompressobj(-15);raw=dec.decompress(bg[offset+2:]);assert dec.eof
            length=len(bg)-offset-len(dec.unused_data)
            assert len(raw)<=65536
            fields += [pointer,length,len(raw),fnv(raw)]
        fields += list(struct.unpack_from('>III',bg,entry+12))
        records.append(struct.pack('<16I',*fields))
    portal=struct.unpack_from('>I',bg,8)[0]&0xffffff
    edges=bytearray()
    while struct.unpack_from('>I',bg,portal)[0]:
        edges += bytes(bg[portal+4:portal+8]);portal+=8
    row=next(r for r in rows if 'Tbg_dam_all_p_stanZ.bin' in r[2])
    start,size=map(int,row[:2]);stan=bytes(rom[start:start+size])
    payload=b''.join(records)+edges+stan
    path.write_bytes(struct.pack('<8s5I',b'EOGWORLD',1,len(records),len(edges)//4,len(stan),fnv(payload))+payload)
    return {'rooms':len(records),'portals':len(edges)//4,'collision_packed_bytes':len(stan)}


def create_walk_reference(rom_path, path):
    """Test-only linked-tile route; never included in console package."""
    from collections import deque
    rom=canonical_rom(rom_path)
    data=zlib.decompress(rom[8720306:8720304+41952],-15)
    first=struct.unpack_from('>I',data,4)[0];at=first;tiles={}
    sizes=[32,32,32,32,40,48,56,64,72,80,88]
    while any(data[at:at+4]):
        n=data[at+6]>>4
        tiles[at]=(data[at+3],[struct.unpack_from('>hhhH',data,at+8+i*8) for i in range(n)])
        at+=sizes[n]
    def inside(points):
        cross=[(q[0]-p[0])*(3949-p[2])-(q[2]-p[2])*(4719-p[0]) for p,q in zip(points,points[1:]+points[:1])]
        return min(cross)>=0 or max(cross)<=0
    start=next(t for t,(room,points) in tiles.items() if room==135 and inside(points))
    queue=deque([start]);previous={start:None};end=None
    while queue:
        tile=queue.popleft();room,points=tiles[tile]
        if room==133 and sum(p[2] for p in points)/len(points)>4050: end=tile;break
        for x,y,z,link in points:
            dest=first-128+link*8
            if link>>4 and dest in tiles and dest not in previous:
                previous[dest]=tile;queue.append(dest)
    assert end is not None
    route=[]
    while end is not None:
        points=tiles[end][1]
        route.append((sum(p[0] for p in points)/len(points),sum(p[2] for p in points)/len(points)))
        end=previous[end]
    path.write_bytes(b''.join(struct.pack('<2f',*point) for point in reversed(route)))


def create_motion(rom_path,path):
    rom=canonical_rom(rom_path)
    entries=bytes(rom[0x124ac0:0x28e980]);table=bytes(rom[0x28e980:0x29d160])
    path.write_bytes(struct.pack('<8s6I',b'EOGGAIT1',1,len(entries),len(table),fnv(entries),fnv(table),0)+entries+table)
