#!/usr/bin/env python3
"""Render a geometry preview; this is not a HOME Menu emulator."""
from pathlib import Path
import math
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[2]
out=ROOT/'native-engine/build/cia';data=np.load(out/'banner-mesh.npz')
colors=np.asarray([[.24,.12,.025],[.92,.65,.16],[.24,.12,.025],[.92,.65,.16]])
triangles=[];normals=[];materials=[]
for i in range(4):
    p=data[f'p{i}'];n=data[f'n{i}'];idx=data[f'i{i}'].reshape(-1,3)
    triangles.extend(p[idx]);normals.extend(n[idx].mean(axis=1));materials.extend([i]*len(idx))
triangles=np.asarray(triangles);normals=np.asarray(normals);materials=np.asarray(materials)
light=np.asarray([-.3,.55,.8]);light/=np.linalg.norm(light)
def render(angle,white=False):
    size=(640,384);c,s=math.cos(angle),math.sin(angle)
    rotation=np.asarray([[c,0,s],[0,1,0],[-s,0,c]])
    points=triangles.copy();n=normals.copy()
    rotating=materials>=0
    points[rotating]=triangles[rotating]@rotation.T;n[rotating]=normals[rotating]@rotation.T
    depth=44.786-points[:,:,2];factor=size[1]/2/math.tan(math.pi/12)
    screen=np.stack([size[0]/2+factor*points[:,:,0]/depth,size[1]/2-factor*(points[:,:,1]-1)/depth],axis=-1)
    image=Image.new('RGB',size,(238,241,245) if white else (9,17,33));draw=ImageDraw.Draw(image)
    diffuse=np.maximum(0,n@light);spec=np.maximum(0,n[:,2])**35*.18
    rgb=np.clip(colors[materials]*(.44+.56*diffuse[:,None])+spec[:,None],0,1)*255
    for index in np.argsort(points[:,:,2].mean(axis=1)):
        if n[index,2]<=0:continue
        draw.polygon([tuple(x) for x in screen[index]],fill=tuple(int(x) for x in rgb[index]))
    return image
render(.18).save(out/'banner-preview.png')
render(.18,True).save(out/'banner-home-preview.png')
# Rotation below is only a geometry inspection preview, not an embedded loop.
frames=[render(2*math.pi*i/60) for i in range(60)]
frames[0].save(out/'banner-preview.gif',save_all=True,append_images=frames[1:],duration=167,loop=0)
print(out/'banner-preview.gif')
