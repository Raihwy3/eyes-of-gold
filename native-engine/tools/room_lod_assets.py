"""Bake planar whole-room LOD maps while retaining every boundary/fold.

Combining vertex-load runs exposes interior edges that the small-run reducer
had to treat as boundaries. Positions weld; authored UV/colour bytes remain
on their original vertices. All topology work happens on the host.
"""
from pathlib import Path
import csv,hashlib,json,struct,subprocess,zlib
import build,stage_assets
from native_fixture import canonical_rom,fnv

def generate():
    out=build.BUILD/'room-lod';out.mkdir(exist_ok=True)
    header=build.BUILD/'include/native_room_lods.h'
    reducer=build.BUILD/'object-lod/reducer.h'
    rompath=build.ROOT/'local/roms/GoldenEye.n64'
    digest=hashlib.sha256(Path(__file__).read_bytes()+Path(stage_assets.__file__).read_bytes()+reducer.read_bytes()+str((rompath.stat().st_size,rompath.stat().st_mtime_ns)).encode()).hexdigest()
    if header.exists() and (out/'samples.bin').exists() and (out/'stamp').exists() and (out/'stamp').read_text()==digest:return
    rom=canonical_rom(rompath)
    jobs=[];binary=bytearray()
    for stage,room,bg,table in stage_assets.rooms(rom):
        vp,dlp,_=struct.unpack_from('>III',bg,table+room*24)
        if not vp or not dlp:continue
        vertices=zlib.decompress(bg[(vp&0xffffff)+2:],-15)
        dl=zlib.decompress(bg[(dlp&0xffffff)+2:],-15)
        nv=len(vertices)//16;loaded=[None]*128;points=[];ids={};tri=[];valid=True
        for i in range(nv):
            xyz=struct.unpack_from('>3h',vertices,i*16)
            if xyz not in ids:ids[xyz]=len(points);points.append(xyz)
        for at in range(0,len(dl)-7,8):
            x,y=struct.unpack_from('>II',dl,at);op=x>>24
            if op==0xb8:break
            if op in (1,6,0xb2):valid=False;break # dynamic matrices, nested lists, UV mutation
            if op==4:
                n=(x&65535)//16;dest=x>>16&15;offset=y&0xffffff
                if y>>24!=14 or offset%16 or dest+n>128 or offset+n*16>len(vertices):valid=False;break
                for i in range(n):loaded[dest+i]=ids[struct.unpack_from('>3h',vertices,offset+i*16)]
            ts=[]
            if op==0xbf:ts=[tuple((y>>s&255)//10 for s in (16,8,0))]
            if op==0xb1:ts=[(y>>(8*j)&15,y>>(8*j+4)&15,x>>(4*j)&15) for j in range(4) if (y>>(8*j)&255)|(x>>(4*j)&15)]
            for t in ts:
                if any(i>=128 or loaded[i] is None for i in t):valid=False;break
                tri.append(tuple(loaded[i] for i in t))
            if not valid:break
        if not valid or not tri or len(tri)>2048 or len(points)>2048:continue
        native=b''.join(struct.pack('<3hH2h4B',*struct.unpack_from('>3hH2h4B',vertices,i*16)) for i in range(nv))
        jobs.append((stage,room,native,points,ids,tri))
        binary+=struct.pack('<2I',len(points),len(tri))
        binary+=b''.join(struct.pack('<3hH2h4B',*p,0,0,0,127,127,127,127) for p in points)
        binary+=b''.join(struct.pack('<3H',*t) for t in tri)
    (out/'input.bin').write_bytes(binary)
    (out/'reduce.cpp').write_text('''#include "reducer.h"
#include <cstdio>
int main(int argc,char**argv){if(argc!=3)return 1;FILE*f=fopen(argv[1],"rb"),*o=fopen(argv[2],"wb");if(!f||!o)return 2;
static OfflineMeshLod::Input v[2048];static OfflineMeshLod::Triangle t[2048],a[2048],b[2048],c[2048];static unsigned ra[2048],rb[2048],rc[2048];unsigned vn,n;
while(fread(&vn,4,1,f)==1){if(fread(&n,4,1,f)!=1||vn>2048||n>2048||fread(v,16,vn,f)!=vn||fread(t,6,n,f)!=n)return 3;
unsigned x=OfflineMeshLod::simplify(t,n,v,a,1,ra,true),y=OfflineMeshLod::simplify(a,x,v,b,2,rb,true),z=OfflineMeshLod::simplify(b,y,v,c,3,rc,true);fwrite(&x,4,1,o);fwrite(&y,4,1,o);fwrite(&z,4,1,o);
for(unsigned i=0;i<vn;i++){unsigned short q=ra[i];fwrite(&q,2,1,o);}for(unsigned i=0;i<vn;i++){unsigned short q=rb[ra[i]];fwrite(&q,2,1,o);}for(unsigned i=0;i<vn;i++){unsigned short q=rc[rb[ra[i]]];fwrite(&q,2,1,o);}}
fclose(f);fclose(o);}
''')
    subprocess.run(['g++','-std=c++20','-O2','-I'+str(build.BUILD/'object-lod'),'-I'+str(build.TARGET/'include'),str(out/'reduce.cpp'),'-o',str(out/'reduce')],check=True)
    subprocess.run([str(out/'reduce'),str(out/'input.bin'),str(out/'output.bin')],check=True)
    data=(out/'output.bin').read_bytes();offset=0;lines=[];records=[];report=[];samples=bytearray()
    for stage,room,native,points,ids,tri in jobs:
        mid,far,distant=struct.unpack_from('<3I',data,offset);offset+=12;n=len(points)
        maps=[struct.unpack_from('<'+str(n)+'H',data,offset+2*n*i) for i in range(3)];offset+=6*n
        if mid==len(tri) and far==mid and distant==far:continue
        assert 0<distant<=far<=mid<=len(tri)
        # The reducer locks open/nonmanifold boundaries. Assert that separately
        # on every exported map; shared room-edge vertices must stay exact.
        edges={}
        for t in tri:
            for a,b in zip(t,(t[1],t[2],t[0])):
                key=tuple(sorted((a,b)));edges[key]=edges.get(key,0)+1
        locked={p for edge,count in edges.items() if count!=2 for p in edge}
        assert all(m[p]==p for m in maps for p in locked)
        for m in maps:
            for t in tri:
                a,b,c=(points[i] for i in t)
                ab=[b[i]-a[i] for i in range(3)];ac=[c[i]-a[i] for i in range(3)]
                normal=(ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0])
                mapped=[points[m[i]] for i in t]
                assert all(sum(normal[i]*(p[i]-a[i]) for i in range(3))==0 for p in mapped), 'Room LOD moved a face out of its original plane'
        targets=[]
        fixture=[]
        for level,m in enumerate(maps):
            coords=[]
            for i in range(len(native)//16):coords.extend(points[m[ids[struct.unpack_from('<3h',native,i*16)]]])
            name=f'native_room_lod_{stage}_{room}_{level}'
            lines.append('static const short '+name+'[]={'+','.join(map(str,coords))+'};');targets.append(name)
            fixture.append(struct.pack('<'+str(len(coords))+'h',*coords))
        records.append('{'+f'{room},{fnv(native)}u,{len(native)//16},'+','.join(targets)+'}')
        report.append(dict(room=room,triangles=[len(tri),mid,far,distant]))
        samples+=struct.pack('<6I',room,len(native)//16,len(tri),mid,far,distant)+native+b''.join(fixture)
    assert offset==len(data)
    lines.append('static const struct NativeRoomLodRecord native_room_lods[]={'+','.join(records or ['{0}'])+'};')
    lines.append('static const unsigned native_room_lod_count='+str(len(records))+';')
    header.write_text('\n'.join(lines)+'\n');(out/'samples.bin').write_bytes(samples);(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');(out/'stamp').write_text(digest)
    print('Whole-room planar LODs:',len(records),'rooms, triangles',[sum(r['triangles'][i] for r in report) for i in range(4)])
