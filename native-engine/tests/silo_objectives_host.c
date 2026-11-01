/* Original Silo deposit handler/evaluator with ROM-derived objective records. */
#include <ultra64.h>
#include <bondtypes.h>
#include "game/objective_status.h"
#include "native_test_assert.h"
#include <string.h>
struct objective_entry *objective_ptrs[OBJECTIVES_MAX];
OBJECTIVESTATUS objectiveStatuses[OBJECTIVES_MAX];
int g_savelogObjOnce;
struct stagesetup g_CurrentSetup;
struct criteria_deposit *ptr_last_deposit_in_room_subobject_entry_type21;
static unsigned flags;
s32 get_keyanalyzer_flag(void){return 0;}
s32 get_debug_all_obj_complete_flag(void){return 0;}
bool chrHasStageFlag(ChrRecord *chr,s32 wanted){(void)chr;return (flags&(unsigned)wanted)==(unsigned)wanted;}
ObjectRecord *objFindByTagId(s32 tag){(void)tag;return NULL;}
bool objIsHealthy(ObjectRecord *obj){(void)obj;return 0;}
bool bondinvHasPropInInv(PropRecord *prop){(void)prop;return 0;}
void osSyncPrintf(const char *format,...){(void)format;abort();}
#include "silo-objective-functions.h"
static unsigned word(const unsigned char *p){unsigned v;memcpy(&v,p,4);return v;}
static void putword(unsigned char *p,unsigned v){memcpy(p,&v,4);}
int main(int argc,char **argv) {
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));long len=ftell(f);rewind(f);
 unsigned char *setup=malloc((size_t)len);assert(setup && fread(setup,1,(size_t)len,f)==(size_t)len);fclose(f);
 unsigned start=word(setup+12),end=word(setup+8),parent=0,owner=0,criteria[4],n=0,maxpad=0,failures=0;
 for(unsigned at=start;at<end;) {
  PropDefHeaderRecord *h=(void *)(setup+at);
  if(h->type==PROPDEF_OBJECTIVE_START)parent=at;
  if(h->type==PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM) {
   assert(n<4 && parent && (!owner || owner==parent));owner=parent;criteria[n++]=at;
   unsigned pad=word(setup+at+8);assert(isNotBoundPad(pad));if(pad>maxpad)maxpad=pad;
  }
  at+=4*(unsigned)sizepropdef(h);
 }
 assert(n==4 && maxpad<10000);objective_ptrs[0]=(void *)(setup+owner);
 for(unsigned at=owner;;) {
  PropDefHeaderRecord *h=(void *)(setup+at);
  if(h->type==PROPDEF_OBJECTIVE_END)break;
  if(h->type==PROPDEF_OBJECTIVE_COMPLETE_CONDITION)flags|=word(setup+at+4);
  if(h->type==PROPDEF_OBJECTIVE_FAIL_CONDITION)failures|=word(setup+at+4);
  at+=4*(unsigned)sizepropdef(h);
 }
 assert(!(flags&failures));
 PadRecord *pads=calloc(maxpad+1,sizeof *pads);assert(pads);g_CurrentSetup.pads=pads;
 struct criteria_deposit deposits[4]={0};StandTile tiles[4]={0};
 for(unsigned i=0;i<n;i++) {
  unsigned at=criteria[i];assert(!word(setup+at+12) && !word(setup+at+16));
  deposits[i].weaponnum=(s32)word(setup+at+4);deposits[i].padid=(s32)word(setup+at+8);
  tiles[i].room=17+i;pads[deposits[i].padid].stan=&tiles[i];
  set_parent_cur_obj_deposited_in_room(&deposits[i]);
 }
 assert(get_status_of_objective(0)==OBJECTIVESTATUS_INCOMPLETE);
 objectivestatusCheckDeposit(deposits[0].weaponnum+1,tiles[0].room);
 objectivestatusCheckDeposit(deposits[0].weaponnum,250);
 for(unsigned i=0;i<n;i++)assert(!deposits[i].flag);
 pads[deposits[0].padid].stan=NULL;
 objectivestatusCheckDeposit(deposits[0].weaponnum,tiles[0].room);assert(!deposits[0].flag);
 pads[deposits[0].padid].stan=&tiles[0];
 for(unsigned i=0;i<n;i++) {
  objectivestatusCheckDeposit(deposits[i].weaponnum,tiles[i].room);
  for(unsigned j=0;j<n;j++){assert(deposits[j].flag==(j<=i));putword(setup+criteria[j]+12,(unsigned)deposits[j].flag);}
  assert(get_status_of_objective(0)==(i+1==n?OBJECTIVESTATUS_COMPLETE:OBJECTIVESTATUS_INCOMPLETE));
 }
 objectivestatusCheckDeposit(deposits[0].weaponnum,tiles[0].room);
 assert(get_status_of_objective(0)==OBJECTIVESTATUS_COMPLETE);
 if(failures){flags|=failures;assert(get_status_of_objective(0)==OBJECTIVESTATUS_FAILED);}
 free(pads);free(setup);
 puts("PASS original Silo explosive objective: all four 5-word criteria, linked registration, wrong weapon/room/missing collision rejection, partial/full placement and failure flags");
}
