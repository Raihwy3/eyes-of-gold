"""Bake whole-part coordinate-collapse maps from the user's verified cartridge.
Original vertex attributes and GBI stay intact; all occurrences of a geometric
point move together, including occurrences separated by vertex-load packets.
"""
from pathlib import Path
import hashlib,json,struct,subprocess,zlib,csv,collections
import build
import prop_proxy
from native_fixture import canonical_rom,fnv

def generate():
    out=build.BUILD/'object-lod';out.mkdir(exist_ok=True)
    header=build.BUILD/'include/native_object_lods.h'
    rompath=build.ROOT/'local/roms/GoldenEye.n64'
    if not rompath.exists():header.write_text('static const struct NativeBakedLodRecord native_baked_lods[1]={{0}};\nstatic const unsigned native_baked_lod_count=0;\n');return
    source=build.TARGET/'include/native_mesh_lod.h'
    digest=hashlib.sha256(Path(__file__).read_bytes()+Path(prop_proxy.__file__).read_bytes()+source.read_bytes()+str((rompath.stat().st_size,rompath.stat().st_mtime_ns)).encode()).hexdigest()
    stamp=out/'stamp'
    if header.exists() and stamp.exists() and (out/'sample.bin').exists() and (out/'report.json').exists() and stamp.read_text()==digest:return
    # The same checked reducer, with wider offline indices and bounds. Runtime
    # keeps its small fixed cache; this host-only variant is never instantiated.
    code=source.read_text().replace('EOG_NATIVE_MESH_LOD_H','EOG_OFFLINE_MESH_LOD_H').replace('NativeMeshLod','OfflineMeshLod').replace('using Triangle=NativeSceneryBatches::Triangle;','struct Triangle {uint16_t a,b,c;};').replace('128','2048')
    code=code[:code.index('static_assert(sizeof(OfflineMeshLod::Input)')]+'#endif\n'
    # Cross-joint vertices are immutable. The high flag is host-only and
    # exists solely in reducer input, never in the original game vertices.
    code=code.replace('locks(source,count,v,locked);','locks(source,count,v,locked);for(unsigned i=0;i<2048;i++)if(v[i].flag&0x8000)locked[i]=true;')
    # These are distance meshes. Spend the reduction budget offline and allow
    # greater surface error while preserving boundary/joint/manifold rules.
    code=code.replace('(tier==1?.0004f:tier==2?.0025f:.0064f)','(tier==1?.0025f:tier==2?.01f:.0225f)')
    code=code.replace('count*(tier<3?3u:2u)/4u','count*(tier<3?3u:2u)/5u')
    code=code.replace('unsigned budget=512','unsigned budget=4096')
    code=code.replace('tier==1?.96f:tier==2?.88f:.8f','tier==1?.85f:tier==2?.65f:.55f')
    (out/'reducer.h').write_text(code)
    (out/'reduce.cpp').write_text('''#include "reducer.h"
#include <cstdio>
int main(int argc,char **argv){if(argc!=3)return 1;FILE*f=fopen(argv[1],"rb"),*o=fopen(argv[2],"wb");if(!f||!o)return 2;
static OfflineMeshLod::Input v[2048];static OfflineMeshLod::Triangle t[2048],a[2048],b[2048],c[2048];static unsigned ra[2048],rb[2048],rc[2048];unsigned n,vn;
while(fread(&vn,4,1,f)==1){if(fread(&n,4,1,f)!=1||vn>2048||n>2048||fread(v,16,vn,f)!=vn||fread(t,6,n,f)!=n)return 3;
unsigned x=OfflineMeshLod::simplify(t,n,v,a,1,ra),y=OfflineMeshLod::simplify(a,x,v,b,2,rb),z=OfflineMeshLod::simplify(b,y,v,c,3,rc);fwrite(&x,4,1,o);fwrite(&y,4,1,o);fwrite(&z,4,1,o);
for(unsigned i=0;i<vn;i++){unsigned short q=ra[i];fwrite(&q,2,1,o);}for(unsigned i=0;i<vn;i++){unsigned short q=rb[ra[i]];fwrite(&q,2,1,o);}for(unsigned i=0;i<vn;i++){unsigned short q=rc[rb[ra[i]]];fwrite(&q,2,1,o);}}
fclose(o);fclose(f);}
''')
    subprocess.run(['g++','-std=c++20','-O2','-I'+str(build.TARGET/'include'),str(out/'reduce.cpp'),'-o',str(out/'reduce')],check=True)
    import native_game_assets as assets
    helpers=assets.original_helpers();counts=assets.model_counts();rom=canonical_rom(rompath)
    records=[];binary=bytearray()
    for row in csv.reader((build.UP/'scripts/filelist.u.csv').open()):
        name=Path(row[2]).stem
        if name[:1] not in ('P','C') or not name.endswith('Z') or not int(row[1]):continue
        start,size=map(int,row[:2]);raw=zlib.decompress(rom[start+2:start+size],-15)
        sizes=next((counts[s.lower()] for s in (name,name[1:],name[:-1],name[1:-1]) if s.lower() in counts),None)
        if not sizes:continue
        nodes,_=helpers['build_nodes'](raw,len(raw),*sizes)
        if name.startswith('P') and any(n.op==8 for n in nodes.values()):continue
        for node in nodes.values():
            if node.op!=24:continue
            u32=lambda p:struct.unpack_from('>I',raw,p)[0]&0xffffff
            dl,vo=u32(node.data),u32(node.data+8);nv=struct.unpack_from('>H',raw,node.data+12)[0]
            if not dl or not vo or not nv or nv>2048:continue
            end=helpers['gdl_end'](raw,len(raw),dl);loaded=[None]*128;points=[];ids={};original_map={};tri=[];matrix=0;valid=True;protected=set();mixed=0
            for at in range(dl,end,8):
                x,y=struct.unpack_from('>II',raw,at);op=x>>24
                if op==1:matrix=y
                if op==4:
                    count=(x&65535)//16;dest=(x>>16)&15;offset=y&0xffffff
                    if y>>24!=4 or offset%16 or offset//16+count>nv or dest+count>128:valid=False;break
                    for i in range(count):
                        index=offset//16+i;xyz=struct.unpack_from('>3h',raw,vo+16*index);key=(matrix,xyz)
                        if key not in ids:ids[key]=len(points);points.append(xyz)
                        point=ids[key]
                        if index in original_map and original_map[index]!=point:valid=False
                        original_map[index]=point;loaded[dest+i]=point
                ts=[]
                if op==0xbf:ts=[((y>>16&255)//10,(y>>8&255)//10,(y&255)//10)]
                if op==0xb1:ts=[(y>>(8*j)&15,y>>(8*j+4)&15,x>>(4*j)&15) for j in range(4) if ((y>>(8*j)&255)|(x>>(4*j)&15))]
                if op==6:valid=False # Nested lists need a whole-tree compiler.
                for t in ts:
                    if any(k>=128 or loaded[k] is None for k in t):valid=False;break
                    triangle=tuple(loaded[k] for k in t)
                    # Mixed transform domains must not share a collapse.
                    if len({next(k[0] for k,v in ids.items() if v==p) for p in triangle})!=1:
                        protected.update(triangle);mixed+=1;continue
                    tri.append(triangle)
                if not valid:break
            if not valid or not tri or len(tri)>2048 or len(points)>2048:continue
            native_vertices=b''.join(struct.pack('<3hH2h4B',*struct.unpack_from('>3hH2h4B',raw,vo+16*i)) for i in range(nv))
            rec=dict(name=name,node=node.data,nv=nv,points=points,original_map=original_map,triangles=len(tri),mixed=mixed,protected=protected,vertices=native_vertices,tri=tri)
            records.append(rec);binary+=struct.pack('<2I',len(points),len(tri))
            binary+=b''.join(struct.pack('<3hH2h4B',*p,0x8000 if i in protected else 0,0,0,127,127,127,127) for i,p in enumerate(points))
            binary+=b''.join(struct.pack('<3H',*t) for t in tri)
    (out/'input.bin').write_bytes(binary)
    subprocess.run([str(out/'reduce'),str(out/'input.bin'),str(out/'output.bin')],check=True)
    keys=collections.Counter((fnv(r['vertices']),r['nv']) for r in records)
    data=(out/'output.bin').read_bytes();offset=0;lines=[];rows=[];report=[]
    for rec in records:
        mid,far,distant=struct.unpack_from('<3I',data,offset);offset+=12;n=len(rec['points'])
        maps=[struct.unpack_from('<'+str(n)+'H',data,offset+2*n*i) for i in range(3)];offset+=6*n
        proxy=prop_proxy.maps_for(rec['name'],rec['points'],rec['tri'],rec['protected'])
        proxy_used=False
        if proxy:
            counts=[prop_proxy.surviving(rec['tri'],m,rec['points']) for m in proxy]
            if 0<counts[1]<=counts[0]<far and counts[0]<=mid:
                maps=[maps[0],*proxy];far,distant=counts;proxy_used=True
        assert 0<distant<=far<=mid<=rec['triangles']
        assert all(mapping[p]==p for mapping in maps for p in rec['protected']), 'LOD moved a cross-joint vertex'
        if mid==rec['triangles'] and far==mid and distant==far:continue
        if keys[(fnv(rec['vertices']),rec['nv'])]!=1:continue
        targets=[];fixturecoords=[]
        for level,mapping in enumerate(maps):
            coords=[]
            for i in range(rec['nv']):
                original=struct.unpack_from('<3h',rec['vertices'],16*i)
                coords.extend(rec['points'][mapping[rec['original_map'][i]]] if i in rec['original_map'] else original)
            fixturecoords.append(coords)
            label=f'native_baked_lod_{len(rows)}_{level}';lines.append('static const short '+label+'[] = {'+','.join(map(str,coords))+'};');targets.append(label)
        rows.append('{'+f'{fnv(rec["vertices"])}u,{rec["nv"]},'+','.join(targets)+'}')
        report.append(dict(model=rec['name'],node=rec['node'],triangles=[v+rec['mixed'] for v in (rec['triangles'],mid,far,distant)],proxy=proxy_used))
        if rec['name']=='PmiltruckZ' and rec['triangles']>100:
            (out/'sample.bin').write_bytes(struct.pack('<5I',rec['nv'],rec['triangles'],mid,far,distant)+rec['vertices']+b''.join(struct.pack('<'+str(len(c))+'h',*c) for c in fixturecoords))

    assert offset==len(data)
    lines.append('static const struct NativeBakedLodRecord native_baked_lods[] = {'+','.join(rows)+'};')
    lines.append('static const unsigned native_baked_lod_count='+str(len(rows))+';')
    header.write_text('\n'.join(lines)+'\n');(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');stamp.write_text(digest)
    print('Baked whole-part object LODs:',len(rows),'parts')
