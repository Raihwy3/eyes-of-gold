#!/usr/bin/env python3
"""Build Eyes of Gold's original 3D HOME Menu banner.

Mesh construction adapted from ThieviusCTR's banner tools. Uses pycgfx (MIT).
No CANM loop: HOME Menu applies its own microphone/motion reaction to the model.
"""
from pathlib import Path
import argparse,base64,importlib.util,json,os,sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
from matplotlib import pyplot as plt
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties
from shapely.geometry import Polygon,MultiPolygon
from shapely.geometry.polygon import orient
from shapely.affinity import scale,translate
import mapbox_earcut
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
def polygons(text,font,width,center_y):
    shapes=None
    for contour in TextPath((0,0),text,size=10,prop=FontProperties(fname=font)).to_polygons():
        p=Polygon(contour)
        if not p.is_valid:p=p.buffer(0)
        shapes=p if shapes is None else shapes.symmetric_difference(p)
    from shapely.affinity import scale,translate
    lo,bot,hi,top=shapes.bounds;factor=width/(hi-lo)
    shapes=scale(shapes,factor,factor,origin=(0,0))
    shapes=translate(shapes,xoff=-(lo+hi)*factor/2,yoff=center_y-(bot+top)*factor/2)
    return shapes.simplify(.025,preserve_topology=True)

def geometry(shapes,back,front):
    vertices=[];normals=[];indices=[]
    def vertex(p,n):vertices.append(p);normals.append(n);return len(vertices)-1
    for polygon in ([shapes] if isinstance(shapes,Polygon) else list(shapes.geoms)):
        polygon=orient(polygon,sign=1)
        rings=[np.asarray(r.coords[:-1],dtype=np.float32) for r in [polygon.exterior,*polygon.interiors]]
        points=np.concatenate(rings);ends=np.cumsum([len(r) for r in rings],dtype=np.uint32)
        triangles=mapbox_earcut.triangulate_float32(points,ends).reshape(-1,3)
        front_ids=[vertex([float(x),float(y),front],[0,0,1]) for x,y in points]
        back_ids=[vertex([float(x),float(y),back],[0,0,-1]) for x,y in points]
        for triangle in triangles:
            a,b,c=map(int,triangle)
            pa,pb,pc=points[[a,b,c]]
            if (pb[0]-pa[0])*(pc[1]-pa[1])-(pb[1]-pa[1])*(pc[0]-pa[0])<0:b,c=c,b
            indices.extend([front_ids[a],front_ids[b],front_ids[c],back_ids[c],back_ids[b],back_ids[a]])
        for ring in rings:
            for a,b in zip(ring,np.roll(ring,-1,axis=0)):
                delta=b-a;length=np.linalg.norm(delta)
                if length<1e-6:continue
                normal=[float(delta[1]/length),float(-delta[0]/length),0]
                ids=[vertex([float(p[0]),float(p[1]),z],normal) for p,z in ((a,back),(b,back),(b,front),(a,front))]
                indices.extend([ids[0],ids[1],ids[2],ids[0],ids[2],ids[3]])
    return np.asarray(vertices,dtype='<f4'),np.asarray(normals,dtype='<f4'),np.asarray(indices,dtype='<u2')

def eye_shapes():
    # Trace the existing custom artwork into geometry; black/transparent pixels
    # never become a banner plane. Preserve holes in the eye and ornament.
    image=Image.open(ROOT/'native-engine/cia/eye-transparent.png').convert('RGBA')
    image=image.crop(image.getbbox());image.thumbnail((128,128),Image.Resampling.LANCZOS)
    mask=np.asarray(image.getchannel('A'),dtype=np.float32)/255.
    fig,ax=plt.subplots();contours=ax.contourf(mask,[.5,1.5])
    result=None
    for path in contours.get_paths():
        for ring in path.to_polygons():
            p=Polygon(ring)
            if not p.is_valid:p=p.buffer(0)
            if p.area<2:continue
            result=p if result is None else result.symmetric_difference(p)
    plt.close(fig)
    assert result is not None
    result=result.simplify(.6,preserve_topology=True)
    lo,bot,hi,top=result.bounds;factor=12.0/(top-bot)
    result=scale(result,factor,-factor,origin=(0,0))
    return translate(result,xoff=-(lo+hi)*factor/2,yoff=4.0+(bot+top)*factor/2)

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--pycgfx',type=Path,default=Path(os.environ.get('EOG_PYCGFX',str(ROOT/'local/cia-tools/pycgfx'))))
    p.add_argument('--font',default=os.environ.get('EOG_BANNER_FONT','/usr/share/fonts/gsfonts/Z003-MediumItalic.otf'))
    a=p.parse_args();out=ROOT/'native-engine/build/cia';out.mkdir(parents=True,exist_ok=True)
    eye=eye_shapes();title=polygons('Eyes of Gold',a.font,25,-5.8)
    meshes=[]
    for shape in (eye,title):
        positions,normals,indices=geometry(shape,-.4,.35)
        triangles=indices.reshape(-1,3)
        is_cap=np.abs(normals[triangles[:,0],2])>.5
        # Separate side/cap materials without duplicating complete extrusions.
        for selection in (~is_cap,is_cap):
            chosen=triangles[selection].reshape(-1)
            used,remap=np.unique(chosen,return_inverse=True)
            meshes.append((positions[used],normals[used],remap.astype('<u2')))

    blob=bytearray();views=[];accessors=[]
    def accessor(values,kind,component):
        while len(blob)%4:blob.append(0)
        at=len(blob);blob.extend(values.tobytes());vi=len(views);views.append({'buffer':0,'byteOffset':at,'byteLength':len(blob)-at})
        item={'bufferView':vi,'componentType':component,'count':len(values),'type':kind}
        if kind=='VEC3':item.update(min=values.min(axis=0).tolist(),max=values.max(axis=0).tolist())
        accessors.append(item);return len(accessors)-1
    primitives=[]
    for i,(positions,normals,indices) in enumerate(meshes):
        assert len(positions)<65536
        assert np.isfinite(positions).all() and indices.max()<len(positions)
        primitives.append({'attributes':{'POSITION':accessor(positions,'VEC3',5126),'NORMAL':accessor(normals,'VEC3',5126)},'indices':accessor(indices,'SCALAR',5123),'material':i%2})
    colors=[(.24,.12,.025,1),(.92,.65,.16,1)]
    model={'asset':{'version':'2.0','generator':'Eyes of Gold original extruded banner'},'scene':0,'scenes':[{'nodes':[0]}],'nodes':[{'name':'EyesOfGold','mesh':0}],'meshes':[{'name':'EyesOfGold','primitives':primitives}],'materials':[{'name':name,'pbrMetallicRoughness':{'baseColorFactor':list(color),'metallicFactor':.35,'roughnessFactor':.5}} for name,color in zip(('Gold edge','Gold face'),colors)],'buffers':[{'byteLength':len(blob),'uri':'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()}],'bufferViews':views,'accessors':accessors}
    gltf=out/'banner-3d.gltf';gltf.write_text(json.dumps(model,separators=(',',':')))
    sys.path.insert(0,str(a.pycgfx.resolve()))
    spec=importlib.util.spec_from_file_location('eog_banner_converter',a.pycgfx/'main.py');converter=importlib.util.module_from_spec(spec);spec.loader.exec_module(converter)
    import gltflib
    cgfx=converter.convert_gltf(gltflib.GLTF.load(str(gltf),load_file_resources=True))
    assert not cgfx.data.skeletal_animations.dict.nodes[1:]
    data=converter.write(cgfx)
    assert data[:4]==b'CGFX' and len(data)<=0x80000 and b'CANM' not in data
    target=ROOT/'native-engine/cia/banner.cgfx';target.write_bytes(data)
    np.savez(out/'banner-mesh.npz',**{f'{key}{i}':v for i,mesh in enumerate(meshes) for key,v in zip(('p','n','i'),mesh)})
    report={'cgfx_bytes':len(data),'triangles':sum(len(m[2])//3 for m in meshes),'vertices':sum(len(m[0]) for m in meshes),'transparent_background':True,'automatic_rotation_loop':False,'microphone_reaction':'HOME Menu model transform; requires console test','title':'Eyes of Gold','source_artwork':'native-engine/cia/eye-transparent.png','pycgfx':'https://github.com/skyfloogle/pycgfx'}
    (out/'banner-3d-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
