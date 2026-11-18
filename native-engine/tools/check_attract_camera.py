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
    replay=build.source_path(Path('src/game/ramromreplay.c')).read_text()
    front=build.source_path(Path('src/game/front.c')).read_text()
    assert 'select_ramrom_to_play();' in front
    assert 'attract demo stages are not converted' not in front
    camera=build.source_path(Path('src/fr.c')).read_text()
    assert camera==(build.UP/'src/fr.c').read_text(), 'Projection must retain the authored near/far planes used by fog'
    rom=out/'native-cart.bin';rom.write_bytes(canonical_rom(build.ROOT/'local/roms/GoldenEye.n64'))
    check_rom_table_loader(out)
    source=r'''
#include <ultra64.h>
#include <macro.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ramromreplay.h"
#include "joy.h"
#include "native_demo.h"
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
s32 g_GlobalTimer,g_ClockTimer;
u64 g_randomSeed;
s32 prev_keypresses;
struct ramrom_seed {u8 speedframes,count,randseed,check;};
struct ramrom_blockbuf {s8 stick_x,stick_y;u8 button_low,button_high;};
struct ramrom_seed *ramrom_blkbuf_2;
struct ramrom_blockbuf *ramrom_blkbuf_3;
static unsigned fades,starts,drifts,stops[4];
static int timer_stop_tick;
static int cont_index;
static u16 physical_buttons;
void native_demo_replay_begin(void){starts++;}
void native_demo_replay_seed(unsigned expected,unsigned actual){drifts+=expected!=actual;}
void native_demo_replay_stop(int reason){assert(reason>=0 && reason<4);stops[reason]++;if(reason==NATIVE_DEMO_TIMER)timer_stop_tick=g_GlobalTimer;}
static void ramromFadeToTitle(void){fades++;}
void joySetContDataIndex(s32 n){cont_index=n;}
u16 joyGetButtonsPressedThisFrame(s8 n,u16 mask){assert(n==0 && cont_index==0);return physical_buttons&mask;}
void updateFrameCounters(s32 n){g_ClockTimer=n;g_GlobalTimer+=n;}
static void *romCopyAligned(void *dst,void *src,int size) {
    uintptr_t at=(uintptr_t)src-0x10000000u;
    unsigned offset=at&1;at&=~(uintptr_t)1;
    dst=(void *)(((uintptr_t)dst+15)&~(uintptr_t)15);
    size=(size+offset+15)&~15;
    assert(at>=begin && at+size<=end+15);
    assert(!fseek(cart,at,SEEK_SET));assert(fread(dst,1,size,cart)==(unsigned)size);
    return (u8 *)dst+offset;
}
void set_solo_and_ptr_briefing(enum LEVELID n){selected_stage=n;}
void set_selected_difficulty(enum DIFFICULTY n){selected_difficulty=n;}
static void frontChangeMenu(MENU n,int r){selected_menu=n;reload=r;}
void guMtxIdentF(float m[4][4]) {memset(m,0,64);for(int i=0;i<4;i++)m[i][i]=1;}
'''
    source+=function(replay,'static void ramromFixupEndian(')+'\n'
    source+=function(replay,'void replay_recorded_ramrom_at_address(')+'\n'
    source+=function(replay,'s32 ramrom_replay_handler(struct contsample *arg0, s32 arg1)\n{')+'\n'
    original=(build.UP/'src/game/ramromreplay.c').read_text()
    source+=function(original,'s32 ramrom_replay_handler(struct contsample *arg0, s32 arg1)\n{').replace('ramrom_replay_handler(', 'reference_replay_handler(',1)+'\n'
    source+=function(replay,'void iterate_ramrom_entries_handle_camera_out(')+'\n'
    perspective=(build.UP/'src/libultra/gu/perspective.c').read_text()
    source+=function(perspective,'void guPerspectiveF(')+'\n'
    source+=r'''
int main(int argc,char **argv) {
    assert(argc==4 && sizeof(ramromfilestructure)==0xe8);
    cart=fopen(argv[1],"rb");assert(cart);begin=strtoul(argv[2],0,10);end=begin+strtoul(argv[3],0,10);
    replay_recorded_ramrom_at_address((void *)(uintptr_t)(0x10000000u+begin));
    ramromfilestructure *h=ptr_active_demofile;
    assert(g_ramromPlayBackFlag && selected_menu==MENU_RUN_STAGE && reload && starts==1);
    assert(selected_stage==h->stagenum && selected_stage>0 && selected_stage<60);
    assert(selected_difficulty==h->difficulty && selected_difficulty<=2);
    assert(h->numplayers==0 && h->size_cmds>0 && h->size_cmds<=4);
    assert(h->filesize>0 && h->filesize<=(int)(end-begin));
    unsigned at=begin+sizeof *h,packets=0;
    struct contsample samples[CONTSAMPLE_LEN]={0};int sample_index=0;
    for(;;) {
        unsigned char seed[4],data[256];assert(at+4<=end);
        assert(!fseek(cart,at,SEEK_SET));assert(fread(seed,1,4,cart)==4);
        if(!seed[0] && !seed[1]) {
            iterate_ramrom_entries_handle_camera_out();assert(stops[NATIVE_DEMO_END]==1);break;
        }
        unsigned bytes=seed[1]*h->size_cmds*4;
        assert(bytes<=sizeof data && at+4+bytes<=end);
        assert(fread(data,1,bytes,cart)==bytes);
        unsigned char sum=seed[0]+seed[1]+seed[2];
        for(unsigned i=0;i<bytes;i++)sum+=data[i];
        assert(sum==seed[3]);
        iterate_ramrom_entries_handle_camera_out();
        assert(ramrom_blkbuf_2->check==seed[3]);
        g_randomSeed=(seed[2]+1)&255; /* Deliberately diverge on every packet. */
        if(!packets){unsigned before=fades;reference_replay_handler(samples,sample_index);assert(fades==before+1);}
        unsigned before=fades;
        sample_index=ramrom_replay_handler(samples,sample_index);
        assert(fades==before && cont_index==1);
        if(!packets) {
            ramrom_blkbuf_2->check^=1;
            ramrom_replay_handler(samples,sample_index);assert(fades==before+1 && stops[NATIVE_DEMO_CHECKSUM]==1);
            ramrom_blkbuf_2->check^=1;physical_buttons=A_BUTTON;
            ramrom_replay_handler(samples,sample_index);assert(fades==before+2 && stops[NATIVE_DEMO_INPUT]==1);
            physical_buttons=0;
        }
        at+=(bytes+5)&~1u;packets++;
        assert((uintptr_t)address_demo_loaded==0x10000000u+at);
    }
    assert(packets>0 && drifts>=packets && stops[NATIVE_DEMO_TIMER]==1);
    assert(g_GlobalTimer>=h->totaltime_ms);
    assert(timer_stop_tick>=h->totaltime_ms-60 && timer_stop_tick<h->totaltime_ms-60+256);
    fclose(cart);
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
    printf("PASS retail demo: stage=%d packets=%u ticks=%d; full iterator/handler survives RNG drift, preserves checksum/input/end/timer exits; fog/projection agree\n",selected_stage,packets,g_GlobalTimer);
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
