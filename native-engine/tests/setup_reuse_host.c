/* Exercise the actual reuse scan on native ROM-derived prop streams. */
#include <ultra64.h>
#include <bondtypes.h>
#include "native_test_assert.h"
#include <string.h>
static struct stagesetup g_CurrentSetup;
bool modelmgrCanSlotFitRwdata(Model *model,ModelFileHeader *file){(void)model;(void)file;abort();}
void osSyncPrintf(const char *format,...){(void)format;abort();}
#include "setup-reuse-functions.h"
static unsigned word(const unsigned char *p){unsigned v;memcpy(&v,p,4);return v;}
int main(int argc,char **argv) {
 assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
 assert(!fseek(f,0,SEEK_END));long len=ftell(f);rewind(f);
 unsigned char *file=malloc((size_t)len);assert(file && fread(file,1,(size_t)len,f)==(size_t)len);fclose(f);
 unsigned start=word(file+12),end=word(file+8);assert(start<end && end<=(unsigned)len);
 /* ASan redzone immediately after the end record, before the intro. */
 unsigned char *props=malloc(end-start);assert(props);memcpy(props,file+start,end-start);free(file);
 g_CurrentSetup.propDefs=(void *)props;
 ObjectRecord *off=(void *)1,*any=(void *)1;
 assert(!setupFindObjForReuse(0x7e,&off,&any,0,0,NULL) && !off && !any);
 ObjectRecord *first=NULL;
 for(unsigned at=0,index=0;at<end-start;index++) {
  PropDefHeaderRecord *h=(void *)(props+at);
  assert(setupGetPtrToCommandByIndex((s32)index)==(h->type==PROPDEF_END?NULL:(ObjectRecord *)h));
  if(h->type==PROPDEF_LINK) {
   s32 relative[2];memcpy(relative,props+at+4,sizeof relative);
   for(unsigned j=0;j<2;j++) {
    ObjectRecord *weapon=setupGetPtrToCommandByIndex((s32)index+relative[j]);
    assert(weapon && weapon->type==PROPDEF_COLLECTABLE);
   }
  }
  if(h->type==PROPDEF_COLLECTABLE){ObjectRecord *o=(void *)h;o->prop=NULL;if(!first)first=o;}
  at+=4*(unsigned)sizepropdef(h);
 }
 assert(first && setupFindObjForReuse(PROPDEF_COLLECTABLE,&off,&any,0,0,NULL)==first);
 assert(!setupFindObjForReuse(PROPDEF_COLLECTABLE,&off,&any,1,1,NULL) && !off && !any);
 assert(!setupGetPtrToCommandByIndex(-1));
 free(props);puts("PASS native setup lookup/reuse: exact record indices and dual-weapon targets, exact END termination, absent-type search, first reusable weapon and exhausted weapon search");
}
