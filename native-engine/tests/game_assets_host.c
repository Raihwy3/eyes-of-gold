// clang-format off
#include <ultra64.h>
#include "game/ob.h"
#include "native_assets.h"
#include "native_baked_lod.h"
#include "native_room_packets.h"
#include "native_rare_skin.h"
#include "native_cart.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// clang-format on
extern void obInit(void);
extern fileentry file_resource_table[];
extern s32 file_entry_max;
extern resource_lookup_data_entry resource_lookup_data_array[];
void sysLogPrintf(int level, const char *fmt, ...) {
  (void)level;
  if(!strcmp(fmt,"Mission PP7 finish: %s"))return;
  va_list a;
  va_start(a, fmt);
  vprintf(fmt, a);
  puts("");
  va_end(a);
}
void sysFatalError(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  vfprintf(stderr, fmt, a);
  va_end(a);
  abort();
}
void osSyncPrintf(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  vprintf(fmt, a);
  va_end(a);
}
void *dynAllocate(s32 bytes){static unsigned long long storage[64];if(bytes<0 || bytes>(s32)sizeof storage)abort();return storage;}
static int check_room_packet_scope(void) {
    Gfx source[3],commands[4];
    if(native_room_packets_begin(commands,3,source,sizeof source)!=commands)return 60;
    native_room_packets_loaded(3);
    if(native_room_packets_begin(commands,3,source,sizeof source)!=commands+1)return 61;
    const struct NativeRoomPacketScope *p=(void *)commands[0].words.w1;
    unsigned generation=p->generation;
    if(p->base!=source || p->room!=3 || p->bytes!=sizeof source || !generation || (commands[0].words.w0>>24)!=NATIVE_ROOM_PACKET_COMMAND)return 62;
    native_room_packets_loaded(3);
    if(native_room_packets_begin(commands,3,source,sizeof source)!=commands+1)return 63;
    p=(void *)commands[0].words.w1;if(p->generation==generation)return 64;
    if(native_room_packets_begin(commands,256,source,sizeof source)!=commands || native_room_packets_begin(commands,3,NULL,sizeof source)!=commands || native_room_packets_begin(commands,3,source,1)!=commands)return 65;
    puts("PASS actual room packet scope: frame-owned pointer/range/load generation, reload invalidation and invalid-room rejection");return 0;
}
static int check_baked_lod(const char *folder) {
    char path[512];snprintf(path,sizeof path,"%s/object-lod/sample.bin",folder);
    FILE *f=fopen(path,"rb");if(!f)return 20;
    unsigned h[5];if(fread(h,4,5,f)!=5 || h[0]>2048 || !(h[4]<=h[3] && h[3]<h[2] && h[2]<h[1]))return 21;
    unsigned char *v=malloc(h[0]*16),*copy=malloc(h[0]*16);short *mid=malloc(h[0]*6),*far=malloc(h[0]*6),*distant=malloc(h[0]*6);
    if(!v||!copy||!mid||!far||!distant)return 22;
    if(fread(v,16,h[0],f)!=h[0] || fread(mid,6,h[0],f)!=h[0] || fread(far,6,h[0],f)!=h[0] || fread(distant,6,h[0],f)!=h[0])return 23;
    fclose(f);memcpy(copy,v,h[0]*16);Gfx commands[4];native_baked_lod_reset();
    if(native_baked_lod_begin(commands,v,h[0])!=commands)return 24;
    for(unsigned tier=1;tier<=3;tier++) {
        native_object_lod_tier=tier;
        if(native_baked_lod_begin(commands,v,h[0])!=commands+1)return 25;
        const struct NativeBakedLodPacket *p=(void *)commands[0].words.w1;
        if(p->base!=v || p->count!=h[0] || memcmp(p->xyz,tier==1?mid:tier==2?far:distant,h[0]*6) || memcmp(v,copy,h[0]*16))return 26;
    }
    v[0]^=1;native_baked_lod_reset();native_object_lod_tier=1;
    if(native_baked_lod_begin(commands,v,h[0])!=commands)return 27;
    native_baked_lod_reset();free(v);free(copy);free(mid);free(far);free(distant);
    printf("PASS whole-part retail object LOD: truck %u -> %u -> %u -> %u, original vertices/UV/normals untouched, full detail and stage-reset rejection\n",h[1],h[2],h[3],h[4]);return 0;
}
static int check_room_lods(const char *folder) {
    char path[512];snprintf(path,sizeof path,"%s/room-lod/samples.bin",folder);
    FILE *f=fopen(path,"rb");if(!f)return 40;unsigned h[6],rooms=0;
    while(fread(h,4,6,f)==6) {
        if(!h[1] || h[1]>4096 || !(h[5]<=h[4] && h[4]<=h[3] && h[3]<=h[2] && h[5]<h[2]))return 41;
        unsigned char *v=malloc(h[1]*16),*copy=malloc(h[1]*16);short *mid=malloc(h[1]*6),*far=malloc(h[1]*6),*distant=malloc(h[1]*6);
        if(!v||!copy||!mid||!far||!distant)return 42;
        if(fread(v,16,h[1],f)!=h[1] || fread(mid,6,h[1],f)!=h[1] || fread(far,6,h[1],f)!=h[1] || fread(distant,6,h[1],f)!=h[1])return 43;
        memcpy(copy,v,h[1]*16);Gfx commands[2];native_baked_lod_reset();
        if(native_room_lod_begin(commands,h[0],v,h[1],0)!=commands || native_room_lod_begin(commands,h[0],v,h[1]+1,1)!=commands)return 44;
        for(unsigned tier=1;tier<=3;tier++) {
            if(native_room_lod_begin(commands,h[0],v,h[1],tier)!=commands+1)return 45;
            const struct NativeBakedLodPacket *p=(void *)commands[0].words.w1;
            if(p->base!=v || p->count!=h[1] || memcmp(p->xyz,tier==1?mid:tier==2?far:distant,h[1]*6) || memcmp(v,copy,h[1]*16))return 46;
        }
        v[0]^=1;native_baked_lod_reset();
        if(native_room_lod_begin(commands,h[0],v,h[1],1)!=commands)return 47;
        free(v);free(copy);free(mid);free(far);free(distant);rooms++;
    }
    fclose(f);if(!rooms)return 48;native_baked_lod_reset();
    printf("PASS whole-room retail LOD: %u rooms select exact mid/far/distant coordinate maps, keep source vertices/UV/colours intact and reject changed sources/counts\n",rooms);return 0;
}
static int check_rare_skin(void) {
  unsigned gold_count=0;
  for(unsigned v=1;v<=10000;v++)gold_count+=native_skin_outcome(v)==1;
  if(gold_count!=100 || native_skin_outcome(0)!=-1 || native_skin_outcome(4294967201u)!=-1 || native_skin_outcome(UINT32_MAX)!=-1)return 70;
  int ids[2]={0,0};const char *names[]={"GwppksilZ","PchrwppksilZ"};
  for(unsigned n=0;n<2;n++)for(int j=1;j<file_entry_max;j++)if(file_resource_table[j].filename && !strcmp(file_resource_table[j].filename,names[n]))ids[n]=j;
  if(!ids[0] || !ids[1])return 71;
  native_assets_stage_skin(0,123);uintptr_t original[2]={(uintptr_t)file_resource_table[ids[0]].hw_address,(uintptr_t)file_resource_table[ids[1]].hw_address};
  gold_count=0;
  for(unsigned i=0;i<2000;i++) {
    int gold=native_assets_stage_skin(1,123+i);gold_count+=gold;
    for(unsigned n=0;n<2;n++) {
      uintptr_t actual=(uintptr_t)file_resource_table[ids[n]].hw_address;
      if((actual!=original[n])!=gold || !native_cart_contains(actual,resource_lookup_data_array[ids[n]].rom_size))return 72;
    }
  }
  if(!gold_count || gold_count>100)return 73;
  native_assets_stage_skin(0,456);
  for(unsigned n=0;n<2;n++)if((uintptr_t)file_resource_table[ids[n]].hw_address!=original[n])return 74;
  puts("PASS rare gold PP7: unbiased 1-of-100 policy, mission-only selection, matched first/third-person variants and title reset to original");return 0;
}
int main(int argc, char **argv) {
  if(argc==3) {if(native_assets_mount(argv[1]))return 10;puts("PASS rejected invalid original-game fixture");return 0;}
  if (argc != 2 || !native_assets_mount(argv[1]))
    return 1;
  int packet_result=check_room_packet_scope();if(packet_result)return packet_result;
  int lod_result=check_baked_lod(argv[1]);if(lod_result)return lod_result;
  lod_result=check_room_lods(argv[1]);if(lod_result)return lod_result;
  char cartpath[512];
  snprintf(cartpath,sizeof cartpath,"%s/native-cart.bin",argv[1]);
  FILE *cart=fopen(cartpath,"rb");if(!cart)return 4;
  const unsigned offsets[]={0x124ac0,0x125000,0x18a400,0x28e930,
                            0x2ebde0,0x2f19a0,0x3b87f0,0x419740};
  uint32_t cached0,file0,cached1,file1;native_cart_stats(&cached0,&file0);
  for(unsigned i=0;i<sizeof offsets/sizeof offsets[0];++i) {
    unsigned char actual[80],expected[80];
    if(fseek(cart,offsets[i],SEEK_SET) || fread(expected,1,80,cart)!=80 ||
       !native_cart_read(0x10000000u+offsets[i],actual,80) || memcmp(actual,expected,80))return 5;
  }
  fclose(cart);native_cart_stats(&cached1,&file1);
  if(cached1-cached0!=8 || file1!=file0)return 6;
  puts("PASS real ROM animation/audio hot ranges: eight frame/sample reads match cartridge bytes with zero file DMA calls");
  obInit();
  int skin_result=check_rare_skin();if(skin_result)return skin_result;
  const char *names[] = {"Ump_setupdishZ", "Ump_setuprefZ", "Ump_setupoatZ", "Ump_setupameZ", "Ump_setupimpZ", "Ump_setupashZ", "Ump_setuparkZ", "Ump_setupsevbZ", "Ump_setuparchZ", "Ump_setupcaveZ", "Ump_setupcrypZ", "bg/bg_dish_all_p.seg", "Tbg_dish_all_p_stanZ", "bg/bg_ref_all_p.seg", "Tbg_ref_all_p_stanZ", "bg/bg_oat_all_p.seg", "Tbg_oat_all_p_stanZ", "bg/bg_ame_all_p.seg", "Tbg_ame_all_p_stanZ", "UsetupcrypZ", "Tbg_cryp_all_p_stanZ", "bg/bg_cryp_all_p.seg", "UsetupaztZ", "Tbg_azt_all_p_stanZ", "bg/bg_azt_all_p.seg", "UsetupcradZ", "Tbg_crad_all_p_stanZ", "bg/bg_crad_all_p.seg", "UsetupcaveZ", "Tbg_cave_all_p_stanZ", "bg/bg_cave_all_p.seg", "UsetupcontrolZ", "Tbg_arec_all_p_stanZ", "bg/bg_arec_all_p.seg", "UsetupjunZ", "Tbg_jun_all_p_stanZ", "bg/bg_jun_all_p.seg", "UsetuptraZ", "Tbg_tra_all_p_stanZ", "bg/bg_tra_all_p.seg", "UsetupdepoZ", "Tbg_depo_all_p_stanZ", "bg/bg_depo_all_p.seg", "UsetuppeteZ", "Tbg_pete_all_p_stanZ", "bg/bg_pete_all_p.seg", "UsetuparchZ", "Tbg_arch_all_p_stanZ", "bg/bg_arch_all_p.seg", "UsetupstatueZ", "Tbg_stat_all_p_stanZ", "bg/bg_stat_all_p.seg", "UsetupsevbZ", "Tbg_sevb_all_p_stanZ", "bg/bg_sevb_all_p.seg", "UsetupsevxbZ", "UsetupdestZ", "Tbg_dest_all_p_stanZ", "bg/bg_dest_all_p.seg", "UsetupsiloZ", "Tbg_silo_all_p_stanZ", "bg/bg_silo_all_p.seg", "UsetupsevbunkerZ", "Tbg_sev_all_p_stanZ", "bg/bg_sev_all_p.seg", "UsetupsevxZ", "Tbg_sevx_all_p_stanZ", "bg/bg_sevx_all_p.seg", "UsetuprunZ", "Tbg_run_all_p_stanZ", "bg/bg_run_all_p.seg", "UsetuparkZ", "Tbg_ark_all_p_stanZ", "bg/bg_ark_all_p.seg", "UsetupdamZ", "Tbg_dam_all_p_stanZ",
                         "bg/bg_dam_all_p.seg", "GwppksilZ", "CarmourguardZ",
                         "PlegalpageZ","PnintendologoZ","PgoldeneyelogoZ",
                         "PwalletbondZ","CheadbrosnanZ"};
  for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
    int id = 0;
    for (int j = 1; j < file_entry_max; j++)
      if (file_resource_table[j].filename &&
          !strcmp(file_resource_table[j].filename, names[i])) {
        id = j;
        break;
      }
    if (id < 1)
      return 2;
    unsigned token = (uintptr_t)file_resource_table[id].hw_address;
    unsigned size = resource_lookup_data_array[id].rom_size;
    if (token < 0x14000000u || !native_cart_contains(token, size))
      return 3;
    printf("PASS original resource lookup %s -> %08x (%u bytes)\n", names[i],
           token, size);
  }
  native_cart_reset();
  return 0;
}
