#include <ultra64.h>
#include <PR/R4300.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/chraidata.h"
#include "game/zlib.h"
#include <stdio.h>
#include <string.h>
#include <random.h>
#include <memp.h>
#include "engine_probe.h"
#include "native_ram.h"
#include "native_cart.h"
#include "hostsched.h"
#include "hostpthread.h"
#include "system.h"
#include "tlb_manage.h"
#include "mixer.h"
#include "romdata.h"
extern Gfx *titleRenderFolderMenuBackgroundLines(Gfx *,u8 *,s32,struct FolderSelectColour *,struct FolderSelectColour *);
extern stagesetup g_CurrentSetup;
extern u64 g_randomSeed;
extern s32 waypointFindRoute(waypoint *,waypoint *,waypoint **,s32);
extern void init_path_table_links(void);
extern u32 decompressdata(u8 *,u8 *,struct huft *);
extern s32 chraiGoToLabel(AIRecord *,s32,u8);
extern MemoryPool g_mempPools[MEMPOOL_COUNT];
extern u8 _bssSegmentEnd;
extern u8 UsetupdamZ[],end_UsetupdamZ[];
extern void romCreateMesgQueue(void);
extern void romCopy(void *,void *,u32);
extern s32 animation_table_ptrs1[],animation_table_ptrs2[];
#define MAX_SETUP 262144
#define MAX_POINTS 512
#define MAX_GROUPS 64
static u8 packed[MAX_SETUP],setup[MAX_SETUP];
static struct huft tables[4096];
static waypoint points[MAX_POINTS+1];
static Gfx menu_commands[4096];
static waygroup groups[MAX_GROUPS+1];
static s32 point_neighbors[MAX_POINTS][17],group_neighbors[MAX_GROUPS][17],members[MAX_GROUPS][MAX_POINTS+1];
static OSMesgQueue queue;
static OSMesg queue_slots[2];
static s16 pcm_left[8]={1,-2,32767,-32768,100,-100,0,123};
static s16 pcm_right[8]={-1,2,-32768,32767,-200,200,0,-123};
static s16 pcm_result[16];
static int check_mixer(void){
 static const s16 expected[16]={1,-1,-2,2,32767,-32768,-32768,32767,100,-200,-100,200,0,0,123,-123};
 mixerInit();
 aSetBufferImpl(0,0x100,0,16);aLoadBufferImpl(osVirtualToPhysical(pcm_left));
 aSetBufferImpl(0,0x200,0,16);aLoadBufferImpl(osVirtualToPhysical(pcm_right));
 aSetBufferImpl(0,0,0x300,16);aInterleaveImpl(0x100,0x200);
 aDMEMMoveImpl(0x300,0x400,32);
 aSetBufferImpl(0,0,0x400,32);aSaveBufferImpl(osVirtualToPhysical(pcm_result));
 int ok=!memcmp(pcm_result,expected,sizeof expected);
 aClearBufferImpl(0x400,32);aSaveBufferImpl(osVirtualToPhysical(pcm_result));
 for(unsigned i=0;i<16;++i)if(pcm_result[i])ok=0;
 mixerDestroy();return ok;
}
static void *queue_sender(void *unused){
 (void)unused;
 for(unsigned i=0;i<512;++i)if(osSendMesg(&queue,(OSMesg)(uintptr_t)(0x1000+i),OS_MESG_BLOCK))return (void *)1;
 return NULL;
}
static int check_queues(void){
 osCreateMesgQueue(&queue,queue_slots,2);OSMesg value=NULL;
 if(osRecvMesg(&queue,&value,OS_MESG_NOBLOCK)!=-1 || osSendMesg(&queue,(void *)1,OS_MESG_NOBLOCK) ||
    osSendMesg(&queue,(void *)2,OS_MESG_NOBLOCK) || osSendMesg(&queue,(void *)3,OS_MESG_NOBLOCK)!=-1 ||
    osRecvMesg(&queue,&value,OS_MESG_NOBLOCK) || value!=(void *)1 ||
    osRecvMesg(&queue,&value,OS_MESG_NOBLOCK) || value!=(void *)2)return 0;
 pthread_t worker;if(pthread_create(&worker,NULL,queue_sender,NULL))return 0;
 unsigned valid=0;
 for(unsigned i=0;i<512;++i){
  if(osRecvMesg(&queue,&value,OS_MESG_BLOCK)==0 && value==(OSMesg)(uintptr_t)(0x1000+i))++valid;
 }
 void *result=(void *)1;if(pthread_join(worker,&result) || result)return 0;
 return valid==512;
}
static unsigned be32(const u8 *p){return (unsigned)p[0]<<24|(unsigned)p[1]<<16|(unsigned)p[2]<<8|p[3];}
static unsigned hash(const void *data,unsigned size){unsigned h=2166136261u;const u8 *p=data;while(size--)h=(h^*p++)*16777619u;return h;}
static void add_word(unsigned *h,unsigned v){for(unsigned i=0;i<4;++i){*h=(*h^(v&255))*16777619u;v>>=8;}}
static int check_memory(void){
 uint32_t off=0;u8 *start=native_ram+NATIVE_HEAP_START;unsigned size=NATIVE_HEAP_END-NATIVE_HEAP_START;
 if(OS_K0_TO_PHYSICAL(start)!=(uintptr_t)start ||
    PHYS_TO_K0(osVirtualToPhysical(&_bssSegmentEnd))!=(uintptr_t)start ||
    (u8 *)tlbmanageGetTlbAllocatedBlock()!=native_ram+NATIVE_HEAP_END ||
    native_ram_translate_kseg((uintptr_t)start)!=(uintptr_t)start ||
    native_ram_translate_kseg(0x88001000u)!=0x08001000u)return 0;
 if(!native_ram_offset(start,16,&off) || off!=NATIVE_HEAP_START || native_ram_resolve(off,16)!=start ||
    native_ram_resolve(0x80000000u+off,16)!=start || native_ram_resolve(0xa0000000u+off,16)!=start ||
    native_ram_resolve((uintptr_t)start,16)!=start || native_ram_resolve(NATIVE_RAM_SIZE-1,2)!=NULL ||
    native_ram_resolve(0x10000000u,1)!=NULL || native_ram_resolve(0x80800000u,1)!=NULL)return 0;
 memset(g_mempPools,0,sizeof(MemoryPool)*MEMPOOL_COUNT);
 g_mempPools[MEMPOOL_TOTAL].start=start;g_mempPools[MEMPOOL_TOTAL].end=start+size;
 s32 sizes[8]={2,0,4,(s32)size-0x50000,6,0x50000,0,0};
 mempSetBankStarts(sizes);mempResetBank(MEMPOOL_STAGE);mempResetBank(MEMPOOL_PERMANENT);
 u8 *a=mempAllocBytesInBank(64,MEMPOOL_STAGE),*b=mempAllocBytesInBank(128,MEMPOOL_STAGE),*p=mempAllocBytesInBank(64,MEMPOOL_PERMANENT);
 if(a!=start || b!=a+64 || p!=native_ram+NATIVE_HEAP_END-0x50000)return 0;
 if(mempAddEntryOfSizeToBank(a,32,MEMPOOL_STAGE)!=MEMP_ADD_ENTRY_NOT_LAST_ALLOCATION ||
    mempAddEntryOfSizeToBank(b,256,MEMPOOL_STAGE)!=MEMP_ADD_ENTRY_SUCCESS ||
    mempGetBankSizeLeft(MEMPOOL_STAGE)!=(s32)size-0x50000-320)return 0;
 mempResetBank(MEMPOOL_STAGE);
 return mempAllocBytesInBank(32,MEMPOOL_STAGE)==a;
}
static int check_graphics(void){
 Gfx commands[2];memset(commands,0,sizeof commands);
 gSPMatrix(commands,(Mtx *)(native_ram+0x60000),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
 gSP1Triangle(commands+1,1,2,3,0);
 if(commands[0].words.w0!=0x01020040u || commands[0].words.w1!=(uintptr_t)(native_ram+0x60000) ||
    commands[1].words.w0!=0xbf000000u || commands[1].words.w1!=0x000a141eu)return 0;
#if __SIZEOF_POINTER__ == 4
 if(commands[0].dma.cmd!=1 || commands[0].dma.par!=2 || commands[0].dma.len!=64 ||
    commands[1].tri.tri.v[0]!=30 || commands[1].tri.tri.v[1]!=20 || commands[1].tri.tri.v[2]!=10)return 0;
#endif
 struct FolderSelectColour top={32,64,96},bottom={160,192,255};
 for(unsigned mode=0;mode<2;++mode){
  Gfx *end=titleRenderFolderMenuBackgroundLines(menu_commands,native_ram+0x10000,mode?12:-12,&top,&bottom);
  if(end-menu_commands!=1203)return 0;unsigned images=0,rects=0,colors=0;
  for(Gfx *p=menu_commands;p<end;++p){unsigned op=(unsigned)p->words.w0>>24;
   if(op==0xfd){if(p->words.w1!=(uintptr_t)(native_ram+0x10000))return 0;++images;}
   else if(op==0xfa){if((p->words.w1&255)!=255)return 0;++colors;}
   else if(op==0xe4)++rects;
  }
  if(images!=1 || rects!=299 || colors!=299)return 0;
 }
 return 1;
}
static int list(const u8 *src,unsigned bytes,unsigned offset,s32 *out,unsigned cap){
 for(unsigned i=0;i<cap;++i){if(offset>bytes || bytes-offset<4)return 0;int value=(s32)be32(src+offset);offset+=4;out[i]=value;if(value<0)return 1;}
 return 0;
}
static int build_graph(unsigned bytes,unsigned *point_count,unsigned *group_count){
 if(bytes<40)return 0;unsigned woff=be32(setup),goff=be32(setup+4),n=0,g=0;
 if(woff>=bytes || goff>=bytes)return 0;
 memset(&g_CurrentSetup,0,sizeof g_CurrentSetup);memset(points,0,sizeof points);memset(groups,0,sizeof groups);
 for(unsigned p=woff;p<=bytes && bytes-p>=16;p+=16){int pad=(s32)be32(setup+p);if(pad<0)break;if(n>=MAX_POINTS)return 0;
  waypoint *w=&points[n];w->padID=pad;w->groupNum=(s32)be32(setup+p+8);w->dist=-1;w->neighbours=point_neighbors[n];
  if(!list(setup,bytes,be32(setup+p+4),w->neighbours,17))return 0;++n;
 }
 points[n].padID=-1;
 for(unsigned p=goff;p<=bytes && bytes-p>=12;p+=12){unsigned near=be32(setup+p);if(!near)break;if(g>=MAX_GROUPS)return 0;
  groups[g].neighbours=group_neighbors[g];groups[g].waypoints=members[g];groups[g].dist=-1;
  if(!list(setup,bytes,near,group_neighbors[g],17) || !list(setup,bytes,be32(setup+p+4),members[g],MAX_POINTS+1))return 0;++g;
 }
 if(!n || !g)return 0;
 for(unsigned i=0;i<n;++i)for(unsigned j=0;j<17 && point_neighbors[i][j]>=0;++j)if(point_neighbors[i][j]>=(s32)n)return 0;
 unsigned seen[MAX_POINTS]={0};
 for(unsigned i=0;i<g;++i){for(unsigned j=0;j<17 && group_neighbors[i][j]>=0;++j)if(group_neighbors[i][j]>=(s32)g)return 0;
  for(unsigned j=0;j<MAX_POINTS+1 && members[i][j]>=0;++j){unsigned m=(unsigned)members[i][j];if(m>=n || seen[m])return 0;seen[m]=1;}
 }
 for(unsigned i=0;i<n;++i)if(!seen[i])return 0;
 g_CurrentSetup.pathwaypoints=points;g_CurrentSetup.waypointgroups=groups;
 *point_count=n;*group_count=g;return 1;
}
static int check_assets(const char *probe_path,EngineReport report){
 char path[512];const char *slash=strrchr(probe_path,'/');size_t prefix=slash?(size_t)(slash-probe_path)+1:0;
 if(prefix+sizeof "native-assets.dat">sizeof path)return 0;
 memcpy(path,probe_path,prefix);memcpy(path+prefix,"native-assets.dat",sizeof "native-assets.dat");
 FILE *file=fopen(path,"rb");char magic[8];unsigned count=0,entry[6][8];
 if(!file){report("Native asset fixture missing",0,0);return 0;}
 int ok=fread(magic,1,8,file)==8 && !memcmp(magic,"EOGAST01",8) && fread(&count,4,1,file)==1 && count==6 && fread(entry,32,6,file)==6;
 fclose(file);if(!ok){report("Native asset fixture header",0,0);return 0;}
 const char *names[6]={"Bank Gothic native glyph layout","Zurich Bold native glyph layout","Original animation scalar fixup","Original briefing endian fixup","Original language endian fixup","Original music-table endian fixup"};
 for(unsigned i=0;i<6;++i){
  unsigned *e=entry[i];native_cart_reset();
  ok=e[0]==i+1 && e[2]>=204 && e[3]>0 && e[3]<MAX_SETUP && e[4]>0 && e[4]<MAX_SETUP && e[7]<=1 && native_cart_mount(path,e[1],e[2],e[3]);
  if(!ok){report("Native asset extent",0,i);return 0;}
  romCopy(packed,(void *)(uintptr_t)e[1],e[3]);
  ok=hash(packed,e[3])==e[5];if(!ok){report("Original asset bytes",0,i);return 0;}
  unsigned bytes=e[7]?decompressdata(packed,setup,tables):e[3];
  if(!e[7])memcpy(setup,packed,bytes);
  if(bytes!=e[4]){report("Original asset decode size",0,bytes);return 0;}
  switch(e[0]){
   case 1:case 2:
    if(romdataFontPcSize((u8 *)(uintptr_t)e[1],bytes)!=bytes)return 0;
    romdataFixupFont(setup,bytes);break;
   case 3:romdataFixupAnimationData(setup,bytes,animation_table_ptrs1,animation_table_ptrs2);break;
   case 4:if(bytes!=48)return 0;romdataFixupBriefing(setup);break;
   case 5:romdataFixupLangBank(setup,bytes);break;
   case 6:romdataFixupMusicSeqTable(setup,bytes);break;
   default:return 0;
  }
  ok=hash(setup,bytes)==e[6];report(names[i],ok,bytes);if(!ok)return 0;
 }
 native_cart_reset();return 1;
}
int engine_probe(const char *path,EngineReport report){
 unsigned header[8];char magic[8];FILE *f=fopen(path,"rb");
 if(!f){report("Native engine fixture missing",0,0);return 0;}
 int ok=fread(magic,1,8,f)==8 && !memcmp(magic,"EOGNATV1",8) && fread(header,4,8,f)==8 && header[0]==1 && header[1]<MAX_SETUP && header[2]<MAX_SETUP && header[1]>2 && header[2]>40 && header[5]==88;
 if(ok)ok=fread(packed,1,header[1],f)==header[1] && fgetc(f)==EOF;
 fclose(f);if(!ok){report("Native fixture header",0,0);return 0;}
#if __SIZEOF_POINTER__ == 4
 _Static_assert(sizeof(Gfx)==8,"3DS Gfx must retain original eight-byte commands");
 _Static_assert(sizeof(Vtx)==16,"Original vertex ABI");
 _Static_assert(sizeof(struct huft)==8,"Original ARM32 inflate table ABI");
 _Static_assert(sizeof(ITEM_IDS)==4,"Original four-byte enum ABI");
#endif
 report("Game structure ABI",1,sizeof(Gfx));
 ok=native_ram_init();report("Allocate original RAM arena",ok,NATIVE_RAM_SIZE);if(!ok)return 0;
 ok=check_queues();report("Native OS queues / thread wakeups",ok,512);if(!ok)return 0;
 uint64_t now=sysGetMicroseconds();unsigned ticks=osGetCount();sysSleep(10000);
 uint64_t elapsed=sysGetMicroseconds()-now;unsigned elapsed_ticks=osGetCount()-ticks;
 ok=elapsed>=5000 && elapsed<1000000 && elapsed_ticks>200000 && elapsed_ticks<50000000;
 report("Native clock / original counter rate",ok,elapsed_ticks);if(!ok)return 0;
 ok=check_mixer();report("CPU audio opcodes / ARM pointers",ok,16);if(!ok)return 0;
 ok=check_graphics();report("Original menu / GBI command layout",ok,1203);if(!ok)return 0;
 ok=check_memory();report("Original allocator / relocated RAM",ok,NATIVE_RAM_SIZE);if(!ok)return 0;
 unsigned cart=(unsigned)(uintptr_t)UsetupdamZ;
 ok=cart==0x108c10d0u && (uintptr_t)end_UsetupdamZ-(uintptr_t)UsetupdamZ==header[1];
 report("Original US asset symbol addresses",ok,cart);if(!ok)return 0;
 unsigned packed_hash=hash(packed,header[1]);u8 guard[17];memset(guard,0x5a,sizeof guard);
 ok=native_cart_mount(path,cart,40,header[1]) && !native_cart_mount(path,cart,40,header[1]) &&
    !native_cart_contains(cart-1,1) && !native_cart_contains(cart+header[1],1) &&
    !native_cart_read(cart-1,guard,1) && guard[0]==0x5a;
 report("Native cartridge extent bounds",ok,header[1]);if(!ok)return 0;
 romCreateMesgQueue();memset(packed,0,header[1]);
 romCopy(packed,(void *)(uintptr_t)cart,header[1]);
 ok=hash(packed,header[1])==packed_hash && native_cart_read(cart+7,guard,17) && !memcmp(guard,packed+7,17);
 report("Original ROM copy / native SD DMA",ok,header[1]);native_cart_reset();if(!ok)return 0;
 unsigned bytes=decompressdata(packed,setup,tables);
 ok=bytes==header[2] && hash(setup,bytes)==header[3];report("Original ROM decompressor",ok,bytes);if(!ok)return 0;
 unsigned n=0,g=0;ok=build_graph(bytes,&n,&g) && n==205 && g==23;report("Original Dam navigation data",ok,n);if(!ok)return 0;
 init_path_table_links();report("Original navigation initializer",1,g);
 unsigned checksum=2166136261u;waypoint *route[MAX_POINTS+2];unsigned queries=0;
 for(unsigned a=0;a<n;++a){for(unsigned b=0;b<n;++b){for(unsigned mode=0;mode<2;++mode){unsigned cap=mode?512:6;int count=waypointFindRoute(points+a,points+b,route,(s32)cap+1)-1;
   if(count<0 || count>(s32)cap){report("Original route count",0,queries);return 0;}
   add_word(&checksum,a);add_word(&checksum,b);add_word(&checksum,cap);add_word(&checksum,(unsigned)count);
   for(int k=0;k<count;++k){if(route[k]<points || route[k]>=points+n){report("Original route pointer",0,queries);return 0;}add_word(&checksum,(unsigned)(route[k]-points));}++queries;
  }}
  if(a%16==0)report("Original pathfinder progress",1,a);
 }
 ok=checksum==header[4];report("Original pathfinder 84050 routes",ok,checksum);if(!ok)return 0;
 AIRecord *attack=ailistFindById(6);ok=attack && hash(attack,header[5])==header[6];report("Compiled original attack script",ok,header[5]);if(!ok)return 0;
 unsigned commands=0,offset=0;
 while(offset<header[5]){unsigned op=((u8*)attack)[offset];int size=chraiitemsize((u8*)attack,(s32)offset);
  if(size<1 || (unsigned)size>header[5]-offset){report("Original AI command decoder",0,offset);return 0;}
  if(op==2){int target=chraiGoToLabel(attack,(s32)offset,((u8*)attack)[offset+1]);if(target!=(s32)offset){report("Original label resolver",0,offset);return 0;}}
  offset+=(unsigned)size;++commands;
 }
 report("Original AI decoder and labels",offset==header[5],commands);
 g_randomSeed=0xAB8D9F7781280783ULL;checksum=2166136261u;
 for(unsigned i=0;i<1024;++i)add_word(&checksum,randomGetNext());
 ok=checksum==header[7];report("Original PRNG 1024 draws",ok,checksum);
 return ok && check_assets(path,report);
}
