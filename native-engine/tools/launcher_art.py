#!/usr/bin/env python3
"""Regenerate the original launcher header artwork; committed for font-free builds."""
from pathlib import Path
from PIL import Image
import build,build_cia

def main():
    out=build.BUILD/'cia';out.mkdir(parents=True,exist_ok=True);build_cia.banner(out)
    logo=Image.open(out/'banner.png').convert('RGBA').resize((200,100),Image.Resampling.LANCZOS)
    eye=Image.open(build.TARGET/'cia/eye-transparent.png').convert('RGBA')
    eye=eye.crop(eye.getbbox());eye.thumbnail((32,32),Image.Resampling.LANCZOS)
    small=Image.new('RGBA',(32,32));small.alpha_composite(eye,((32-eye.width)//2,(32-eye.height)//2))
    lines=['/* Generated from custom eye/banner artwork; no game assets. */','#ifndef EOG_LAUNCHER_ART_H','#define EOG_LAUNCHER_ART_H','#include <stdint.h>']
    for name,image in [('native_launcher_logo',logo),('native_launcher_eye',small)]:
        bg=Image.new('RGBA',image.size,(6,8,11,255));bg.alpha_composite(image)
        values=[((r>>3)<<11)|((g>>2)<<5)|(b>>3) for r,g,b,_ in bg.get_flattened_data()]
        lines.append('static const uint16_t '+name+'[]={')
        for i in range(0,len(values),20):lines.append(','.join(hex(v) for v in values[i:i+20])+',')
        lines.append('};')
    lines.append('#endif')
    (build.TARGET/'include/native_launcher_art.h').write_text('\n'.join(lines)+'\n')
if __name__=='__main__':main()
