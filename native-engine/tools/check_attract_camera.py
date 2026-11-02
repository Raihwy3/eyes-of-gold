"""Check ROM replays and consistent projection/fog distance mapping."""
import csv
from pathlib import Path
import build, build_probe
from check_guard_pose import function
from native_fixture import canonical_rom


def check_rom_table_loader(out):
    import rom_data_plan
    plan=rom_data_plan.runtime_plan();rom_data_plan.emit_runtime_header()
    table=next(r for r in plan['rows'] if r['symbol']=='ramrom_table')
    assert table['unit']=='ramromreplay.c' and table['bank']=='cdata' and table['bytes']==120
    assert plan['pointers']==sorted(plan['pointers'],key=lambda r:r['key'])
    symbols={r['link_name'] for r in plan['rows']}
    code=['#include <stdint.h>', '#include <assert.h>']
    for r in plan['rows']:
        code.append('unsigned char '+r['link_name']+'['+str(r['bytes'])+'] __attribute__((aligned(8)));')
    code.append('const void *native_rom_test_symbol(unsigned n){switch(n){')
    for i,g in enumerate(plan['getters']):
        value=g['name'] if g['name'] in symbols else '(uintptr_t)'+str(0x02000000+i*4)+'u'
        code.append('case '+str(i)+':return (const void *)'+value+';')
    code+=['default:return 0;}}', '#define NATIVE_ROM_DATA_TEST', '#include "native_rom_data.c"',
           '#include "native_test_assert.h"', 'int main(int argc,char **argv){assert(argc==2);',
           'if(!native_rom_data_init(argv[1])){fprintf(stderr,"%s\\n",native_rom_data_error());return 1;}',
           'for(unsigned i=0;i<15;i++){uint32_t pointer,lock;',
           'memcpy(&pointer,ramrom_table+i*8,4);memcpy(&lock,ramrom_table+i*8+4,4);',
           'unsigned key=be32(original_cdata+'+str(table['source_offset'])+'+i*8);',
           'assert(pointer==(key?key+0x10000000u:0));',
           'assert(lock==be32(original_cdata+'+str(table['source_offset'])+'+i*8+4));}',
           'puts("PASS actual ROM initializer: demo selection table populated from ROM; all fifteen native cart pointers/locks match");return 0;}']
    test=out/'rom-loader.c';test.write_text('\n'.join(code));binary=out/'rom-loader';inflate=out/'inflate.o'
    flags=['-std=c11','-O2','-I'+str(build.TARGET/'include'),'-I'+str(build.BUILD/'include'),'-I'+str(build.TARGET/'src')]
    build_probe.run(['gcc',*flags,'-DCONVERT_NO_MAIN','-c',build.TARGET/'tests/native_convert_host.c','-o',inflate])
    build_probe.run(['gcc',*flags,'-no-pie',test,inflate,'-lz','-o',binary])
    print(build_probe.run([binary,out]).stdout,end='')


def main():
    out=build.BUILD/'attract-camera';out.mkdir(exist_ok=True)
    replay=(build.UP/'src/game/ramromreplay.c').read_text()
    front=build.source_path(Path('src/game/front.c')).read_text()
    assert 'select_ramrom_to_play();' in front
    assert 'attract demo stages are not converted' not in front
    camera=build.source_path(Path('src/fr.c')).read_text()
    assert camera==(build.UP/'src/fr.c').read_text(), 'Projection must retain the authored near/far planes used by fog'
    rom=out/'native-cart.bin';rom.write_bytes(canonical_rom(build.ROOT/'local/roms/GoldenEye.n64'))
    check_rom_table_loader(out)
    source=r'''
#include <ultra64.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ramromreplay.h"
#include "native_distance_fog.h"
#include "native_test_assert.h"
static FILE *cart;
static unsigned begin,end;
static enum LEVELID selected_stage;
static enum DIFFICULTY selected_difficulty;
static MENU selected_menu;
static int reload;
char ramrom_data_target[0x380] __attribute__((aligned(16)));
u8 *address_demo_loaded;
ramromfilestructure *ptr_active_demofile;
s32 g_ramromPlayBackFlag;
static void *romCopyAligned(void *dst,void *src,int size) {
    uintptr_t at=(uintptr_t)src-0x10000000u;
    assert(at>=begin && at+size<=end);
    assert(!fseek(cart,at,SEEK_SET));assert(fread(dst,1,size,cart)==(unsigned)size);
    return dst;
}
void set_solo_and_ptr_briefing(enum LEVELID n){selected_stage=n;}
void set_selected_difficulty(enum DIFFICULTY n){selected_difficulty=n;}
static void frontChangeMenu(MENU n,int r){selected_menu=n;reload=r;}
void guMtxIdentF(float m[4][4]) {memset(m,0,64);for(int i=0;i<4;i++)m[i][i]=1;}
'''
    source+=function(replay,'static void ramromFixupEndian(')+'\n'
    source+=function(replay,'void replay_recorded_ramrom_at_address(')+'\n'
    perspective=(build.UP/'src/libultra/gu/perspective.c').read_text()
    source+=function(perspective,'void guPerspectiveF(')+'\n'
    source+=r'''
int main(int argc,char **argv) {
    assert(argc==4 && sizeof(ramromfilestructure)==0xe8);
    cart=fopen(argv[1],"rb");assert(cart);begin=strtoul(argv[2],0,10);end=begin+strtoul(argv[3],0,10);
    replay_recorded_ramrom_at_address((void *)(uintptr_t)(0x10000000u+begin));
    ramromfilestructure *h=ptr_active_demofile;
    assert(g_ramromPlayBackFlag && selected_menu==MENU_RUN_STAGE && reload);
    assert(selected_stage==h->stagenum && selected_stage>0 && selected_stage<60);
    assert(selected_difficulty==h->difficulty && selected_difficulty<=2);
    assert(h->numplayers==0 && h->size_cmds>0 && h->size_cmds<=4);
    assert(h->filesize>0 && h->filesize<=(int)(end-begin));
    unsigned at=begin+sizeof *h,packets=0;
    for(;;) {
        unsigned char seed[4],data[256];assert(at+4<=end);
        assert(!fseek(cart,at,SEEK_SET));assert(fread(seed,1,4,cart)==4);
        if(!seed[0] && !seed[1])break;
        unsigned bytes=seed[1]*h->size_cmds*4;
        assert(bytes<=sizeof data && at+4+bytes<=end);
        assert(fread(data,1,bytes,cart)==bytes);
        unsigned char sum=seed[0]+seed[1]+seed[2];
        for(unsigned i=0;i<bytes;i++)sum+=data[i];
        assert(sum==seed[3]);at+=(bytes+5)&~1u;packets++;
    }
    assert(packets>0);fclose(cart);
    for(int aspect=0;aspect<3;aspect++)for(int zoom=0;zoom<2;zoom++)for(int n=2;n<=30;n++) {
        float matrix[4][4],ratio=aspect==0?4.f/3:aspect==1?5.f/4:5.f/3;
        float near=n,far=15000;
        guPerspectiveF(matrix,0,zoom?20:60,ratio,near,far,1);
        for(int scale_index=0;scale_index<3;scale_index++)for(int choice=1;choice<=3;choice++) {
            float scale=scale_index==0?.25f:scale_index==1?1.f:3.f;
            float end=choice==1?4000:choice==2?2200:1200;
            struct NativeDistanceFog fog;
            assert(native_distance_fog_plan(near/scale,far/scale,end,0,&fog));
            float w=end*scale,z=-w*matrix[2][2]+matrix[3][2];
            assert(fabsf(z/w-fog.end_ndc)<2e-7f);
            assert(native_distance_fog_factor(&fog,z,w)>254.f);
            float distance=end*.65f*scale;
            z=-distance*matrix[2][2]+matrix[3][2];
            assert(native_distance_fog_factor(&fog,z,distance)<1.f);
            assert(fog.end_world==end);
        }
    }
    printf("PASS retail demo: stage=%d packets=%u; actual projection and Near/Medium/Far fog boundaries agree at all aspects/zoom\n",selected_stage,packets);
}
'''
    test=out/'check.c';test.write_text(source);binary=out/'check'
    build_probe.run(['gcc',*build_probe.engine_args(True),test,'-no-pie','-Wl,--gc-sections','-lm','-o',binary])
    demos=[row for row in csv.reader((build.UP/'scripts/filelist.u.csv').open()) if row[2].startswith('assets/ramrom/')]
    assert len(demos)==14
    for offset,size,*_ in demos:
        print(build_probe.run([binary,rom,offset,size]).stdout,end='')
    print('PASS all fourteen ROM-backed attract demos: actual endian fixup and launch, complete packet checksums/bounds')


if __name__=='__main__':
    build.prepare();main()
