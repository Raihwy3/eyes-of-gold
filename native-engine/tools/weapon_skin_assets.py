"""Conversion-time gold finish for the silenced PP7, without runtime effects.

Only pre-shaded weapon vertices change. Hands, sleeves, muzzle flash, normals,
texture commands, mesh topology and animation data retain original bytes.
"""
import struct

MODELS={'GwppksilZ':{26,1608},'PchrwppksilZ':{873,878,879,1514}}
# Bright gold highlight from the original gold PP7's model palette.
GOLD=(255,233,119)

def apply(name,raw,native,counts,helpers):
    if name not in MODELS:return native,0
    nodes,_=helpers['build_nodes'](raw,len(raw),*counts)
    wanted=set();protected=set()
    for node in nodes.values():
        if node.op not in (4,24):continue
        pointer=lambda at:struct.unpack_from('>I',raw,at)[0]&0xffffff
        dl=pointer(node.data);vo=pointer(node.data+(12 if node.op==4 else 8))
        n=struct.unpack_from('>H',raw,node.data+(16 if node.op==4 else 12))[0]
        if not vo or not n:continue
        assert vo+n*16<=len(raw)
        textures=set()
        if dl:
            end=helpers['gdl_end'](raw,len(raw),dl)
            for at in range(dl,end,8):
                x,y=struct.unpack_from('>II',raw,at)
                if x>>24==0xc0:textures.add(y&4095)
        vertices=set(range(vo,vo+n*16,16))
        # Exact material whitelist: never infer hand ownership from greyness.
        if textures and textures<=MODELS[name]:wanted.update(vertices)
        else:protected.update(vertices)
    out=bytearray(native);changed=set()
    for vo in wanted-protected:
        r,g,b=raw[vo+12:vo+15]
        assert r==g==b, 'Gold finish encountered normals/coloured hand data'
        for channel,scale in enumerate(GOLD):
            at=vo+12+channel;out[at]=(r*scale+127)//255
            if out[at]!=native[at]:changed.add(at)
    assert changed, 'Silenced PP7 weapon materials no longer match'
    assert all(at in changed for at,(a,b) in enumerate(zip(native,out)) if a!=b)
    assert all(any(vo+12<=at<vo+15 for vo in wanted-protected) for at in changed)
    # Alpha, positions, UVs and every non-weapon byte stay exact.
    return bytes(out),len(wanted-protected)
