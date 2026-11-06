#!/usr/bin/env python3
"""Check console panel bounds and updater/boot chrome without a 3DS."""
from pathlib import Path
import re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def screen(text,width):
    grid=[[' ']*width for _ in range(30)];row=column=0;i=0
    while i<len(text):
        if text[i]=='\x1b':
            m=re.match(r'\x1b\[([0-9;]*)([Hm])',text[i:]);assert m,text[i:i+20]
            if m[2]=='H':row,column=[int(n)-1 for n in m[1].split(';')]
            i+=len(m[0]);continue
        assert 0<=row<30 and 0<=column<width,(row,column,text[i:i+20])
        grid[row][column]=text[i];column+=1;i+=1
    return [''.join(line) for line in grid]
def main():
    with tempfile.TemporaryDirectory(prefix='eog-launcher-ui-') as folder:
        folder=Path(folder);source=folder/'check.c';binary=folder/'check'
        source.write_text(r'''#include "native_launcher_ui.h"
int main(void){
 native_launcher_top("LAUNCHER UPDATE","NEW RELEASE AVAILABLE");native_launcher_bar(20,9,30,0xffffffffu,1);
 putchar('\f');
 native_launcher_bottom("LAUNCHER UPDATE");native_launcher_text(8,5,"INSTALLED",15);native_launcher_text(9,5,NATIVE_RELEASE_TAG,15);
 native_launcher_message(13,"Install this launcher update?\n\nYour ROM, saves and settings\nare kept.",5);
 native_launcher_actions("A: Install     B: Not now");putchar('\f');
 native_launcher_bottom("MISSION PREPARATION");native_launcher_text(8,5,"HARDWARE",32);
 native_launcher_message(17,"Files are prepared from your ROM only when needed.",2);native_launcher_bar(22,5,30,65,100);putchar('\f');
 native_launcher_bottom("LAUNCHER UPDATE");native_launcher_message(13,"A very long SD path /12345678901234567890123456789012345678901234567890123456789 with \x1b[2J control bytes",5);
 native_launcher_actions("A: Continue    B: Exit");
 return native_launcher_percent(0xffffffffu,1)!=100 || native_launcher_percent(12,0)!=0;
}''')
        subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),'-o',str(binary)],check=True)
        output=subprocess.check_output([binary],text=True).split('\f');assert len(output)==4
        top=screen(output[0],50);assert 'FIELD OPERATIONS' in top[3] and '[==============================]' in top[19]
        for index in (1,2,3):
            bottom=screen(output[index],40)
            assert ('LAUNCHER UPDATE' if index!=2 else 'MISSION PREPARATION') in bottom[2] and 'Eyes of Gold' in bottom[25]
            for row in range(2,27):assert bottom[row][2]==' ' and bottom[row][37]==' '
            if index==1:assert 'A: Install' in bottom[20] and 'B: Not now' in bottom[20] and 'are kept.' in '\n'.join(bottom)
            if index==3:assert 'B: Exit' in bottom[20] and 'A: Continue' in bottom[20]
    print('PASS shared launcher UI: top/bottom bounds, simple gold styling, wrapped notices/errors, clear margins and touch-button positions')
if __name__=='__main__':main()
