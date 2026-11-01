"""Precompute bounded Dam triangle-run LODs; never simplify during rendering."""
from pathlib import Path
import csv,struct,zlib,hashlib,json,subprocess,time
import build,stage_assets
from native_fixture import canonical_rom

def generate():
    out=build.BUILD/'bg-lod';out.mkdir(exist_ok=True)
    header=build.BUILD/'include/native_bg_lods.h'
    rompath=build.ROOT/'local/roms/GoldenEye.n64';source=build.TARGET/'include/native_mesh_lod.h'
    digest=hashlib.sha256(Path(__file__).read_bytes()+Path(stage_assets.__file__).read_bytes()+source.read_bytes()+str((rompath.stat().st_size,rompath.stat().st_mtime_ns)).encode()).hexdigest()
    if header.exists() and (out/'stamp').exists() and (out/'report.json').exists() and (out/'samples.bin').exists() and (out/'stamp').read_text()==digest:return
    rom=canonical_rom(rompath)
    jobs=[];seen=set()
    for stage,room,bg,table in stage_assets.rooms(rom):
        pointers=struct.unpack_from('>III',bg,table+room*24)
        if not pointers[0]:continue
        vertices=zlib.decompress(bg[(pointers[0]&0xffffff)+2:],-15)
        for pointer in pointers[1:]:
            if not pointer:continue
            dl=zlib.decompress(bg[(pointer&0xffffff)+2:],-15);loaded=[bytes(16)]*128;available=set();words=[]
            def flush():
                nonlocal words
                tris=[]
                for x,y in words:
                    if x>>24==0xbf:tris.append(tuple(((y>>s)&255)//10 for s in (16,8,0)))
                    else:tris.extend((y>>(8*j)&15,y>>(8*j+4)&15,x>>(4*j)&15) for j in range(4) if (y>>(8*j)&255)|(x>>(4*j)&15))
                words=[]
                if not tris or any(v not in available for t in tris for v in t):return
                raw=b''.join(loaded);triangle=b''.join(bytes(t) for t in tris);key=(triangle,raw)
                if key not in seen:seen.add(key);jobs.append((room,len(tris),raw,triangle))
            for at in range(0,len(dl)-7,8):
                x,y=struct.unpack_from('>II',dl,at);op=x>>24
                if op in (0xbf,0xb1):
                    if len(words)==32:flush()
                    words.append((x,y));continue
                flush()
                if op==0xb8:break
                if op==4:
                    n=(x&65535)//16;dest=(x>>16)&15;offset=y&0xffffff
                    if y>>24!=14 or dest+n>128 or offset+n*16>len(vertices):available.clear();continue
                    for i in range(n):
                        loaded[dest+i]=struct.pack('<3hH2h4B',*struct.unpack_from('>3hH2h4B',vertices,offset+i*16));available.add(dest+i)
                elif op==0xb2:available.clear() # Modified UVs use the original mesh.
            flush()
    (out/'input.bin').write_bytes(b''.join(struct.pack('<I',n)+v+t for _,n,v,t in jobs))
    (out/'reduce.cpp').write_text('''#include "native_mesh_lod.h"
#include <cstdio>
#include <chrono>
int main(int argc,char**argv){if(argc!=3)return 1;FILE*f=fopen(argv[1],"rb"),*o=fopen(argv[2],"wb");if(!f||!o)return 2;
NativeMeshLod::Input v[128];NativeMeshLod::Triangle t[128],a[128],b[128],c[128];unsigned n;
while(fread(&n,4,1,f)==1){if(n>128||fread(v,16,128,f)!=128||fread(t,3,n,f)!=n)return 3;
auto start=std::chrono::steady_clock::now();unsigned mid=NativeMeshLod::simplify(t,n,v,a,1,nullptr,true),far=NativeMeshLod::simplify(a,mid,v,b,2,nullptr,true),distant=NativeMeshLod::simplify(b,far,v,c,3,nullptr,true);
auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
fwrite(&mid,4,1,o);fwrite(&far,4,1,o);fwrite(&distant,4,1,o);fwrite(&ns,8,1,o);fwrite(a,3,mid,o);fwrite(b,3,far,o);fwrite(c,3,distant,o);}fclose(f);fclose(o);}
''')
    subprocess.run(['g++','-std=c++20','-O2','-I'+str(build.TARGET/'include'),str(out/'reduce.cpp'),'-o',str(out/'reduce')],check=True)
    subprocess.run([str(out/'reduce'),str(out/'input.bin'),str(out/'output.bin')],check=True)
    data=(out/'output.bin').read_bytes();offset=0;records=[];samples=bytearray();timings=[]
    def hash_mesh(n,vertices,tri):
        h=0x9e3779b9
        for v in tri:h=((h^v)*16777619)&0xffffffff
        for i in sorted(set(tri)):
            for word in struct.unpack_from('<4I',vertices,16*i):h=((h^word)*16777619)&0xffffffff
        return h^(h>>16)
    for room,n,vertices,tri in jobs:
        mid,far,distant,ns=struct.unpack_from('<IIIQ',data,offset);offset+=20
        a=data[offset:offset+3*mid];offset+=3*mid;b=data[offset:offset+3*far];offset+=3*far;c=data[offset:offset+3*distant];offset+=3*distant;timings.append(ns)
        if mid==n and far==n and distant==n:continue
        h=hash_mesh(n,vertices,tri);records.append((h,n,mid,far,distant,vertices,tri,a,b,c,room))
        samples+=struct.pack('<5I',h,n,mid,far,distant)+vertices+tri+a+b+c
    assert offset==len(data)
    records.sort(key=lambda r:r[0]);lines=[]
    def triangles(data):return '{'+','.join('{'+','.join(map(str,data[i:i+3]))+'}' for i in range(0,len(data),3))+'}'
    for h,n,mid,far,distant,v,t,a,b,c,_ in records:
        values=[]
        for i in range(128):
            x,y,z,flag,u,w,*rgba=struct.unpack_from('<3hH2h4B',v,16*i)
            values.append('{{'+f'{x},{y},{z}'+'},'+str(flag)+',{'+f'{u},{w}'+'},{'+','.join(map(str,rgba))+'}}')
        lines.append('{'+f'{h}u,'+'{'+f'{n},{mid},{far},{distant}'+'},{'+','.join(map(triangles,(t,a,b,c)))+'},{'+','.join(values)+'}}')
    header.write_text('static const NativeMeshLod::Entry native_bg_lods[] = {'+','.join(lines or ['{}'])+'};\nstatic const unsigned native_bg_lod_count='+str(len(records))+';\n')
    (out/'samples.bin').write_bytes(samples)
    report=dict(runs=len(jobs),reduced=len(records),triangles=[sum(r[i] for r in records) for i in (1,2,3,4)],host_compile_ns=sum(timings),worst_host_ns=max(timings,default=0),runtime_compiles=0)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');(out/'stamp').write_text(digest)
    print('Offline supported-stage mesh LODs:',report)
