"""Preview actual native LCD code using privately extracted original fonts."""
import csv,struct
from pathlib import Path
from PIL import Image,ImageDraw
import build,build_probe,bottom_background
from native_fixture import canonical_rom

def main():
    bottom_background.generate()
    out=build.BUILD/'bottom-preview';out.mkdir(exist_ok=True)
    rom=canonical_rom(build.ROOT/'local/roms/GoldenEye.n64')
    rows=list(csv.reader((build.UP/'scripts/filelist.u.csv').open()))
    def resource(name):
        row=next(r for r in rows if len(r)>2 and r[2].endswith('/'+name+'.bin'))
        start,size=map(int,row[:2]);return rom[start:start+size]
    fixture=out/'fonts.bin'
    with fixture.open('wb') as f:
        for name in ('fontZurichBold','fontBankGothic'):
            font=resource(name+'_kerning')+resource(name+'_fontchartable')
            f.write(struct.pack('<169i',*struct.unpack_from('>169i',font)))
            for i in range(94):
                _,baseline,h,w,kern,pixel=struct.unpack_from('>6i',font,676+i*24)
                assert w<=32 and h<=32
                f.write(struct.pack('<4i',w,h,baseline,kern));data=bytearray(1024);stride=(w+7)&~7
                for y in range(h):data[y*32:y*32+w]=font[pixel+y*stride:pixel+y*stride+w]
                f.write(data)
    binary=out/'preview'
    build_probe.run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-I'+str(build.TARGET/'tests/bottom/mock'),'-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),build.TARGET/'tests/bottom/preview.c','-pthread','-lm','-o',binary])
    build_probe.run([binary,fixture,out])
    sheet=Image.new('RGB',(640,264),(22,22,22));draw=ImageDraw.Draw(sheet)
    for i,name in enumerate(('menu','objectives')):
        sheet.paste(Image.open(out/(name+'.ppm')),(i*320,24));draw.text((i*320+8,6),name,fill='white')
    sheet.save(out/'bottom-ui.png');print(out/'bottom-ui.png')
if __name__=='__main__':main()
