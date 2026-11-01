"""Reuse the recreation's original RLE dossier backdrop, from the local ROM."""
import csv,struct
import build
from native_fixture import canonical_rom

def generate():
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    rows=list(csv.reader((build.UP/'scripts/filelist.u.csv').open()))
    row=next(r for r in rows if len(r)>2 and 'usedby7F008DE4.bin' in r[2])
    offset,size=map(int,row[:2]);encoded=rom[offset:offset+size]
    w,h=struct.unpack_from('>HH',encoded);assert (w,h)==(440,299)
    pixels=bytearray()
    for p in range(10,len(encoded)-1,2):
        count,value=encoded[p:p+2];assert count
        pixels.extend([value]*count)
        if len(pixels)>=w*h:break
    assert len(pixels)==w*h
    # Same dark gradient/texture combiner as tools/export_menu.py, sampled to LCD.
    result=[]
    for y in range(240):
        sy=y*h//240
        for x in range(320):result.append(round((20+30*sy/(h-1))*.92+pixels[sy*w+x*w//320]*.08))
    path=build.BUILD/'include/native_bottom_background.h';path.parent.mkdir(exist_ok=True,parents=True)
    path.write_text('/* Generated privately from the verified cartridge. */\nstatic const unsigned char original_bottom_background[320*240]={\n'+','.join(map(str,result))+'\n};\n')
    return path
if __name__=='__main__':generate()
