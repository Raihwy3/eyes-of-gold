/* Exercise the original evaluator with objective records from the user's ROM. */
#include <ultra64.h>
#include <bondtypes.h>
#include "game/objective_status.h"
#include "native_test_assert.h"
#include <string.h>
struct objective_entry *objective_ptrs[OBJECTIVES_MAX];
OBJECTIVESTATUS objectiveStatuses[OBJECTIVES_MAX];
int g_savelogObjOnce;
static ObjectRecord photo_object;
static PropRecord photo_prop;
static unsigned flags,photo_tag;
static int key_copied,healthy=1;
s32 get_keyanalyzer_flag(void){return key_copied;}
s32 get_debug_all_obj_complete_flag(void){return 0;}
bool chrHasStageFlag(ChrRecord *chr,s32 wanted){(void)chr;return (flags&(unsigned)wanted)==(unsigned)wanted;}
ObjectRecord *objFindByTagId(s32 tag){return (unsigned)tag==photo_tag?&photo_object:NULL;}
bool objIsHealthy(ObjectRecord *obj){return obj==&photo_object && healthy;}
bool bondinvHasPropInInv(PropRecord *prop){(void)prop;return 0;}
void osSyncPrintf(const char *format,...){(void)format;abort();}
#include "bunker-objective-functions.h"
static unsigned word(const unsigned char *p){unsigned v;memcpy(&v,p,4);return v;}
static void prepare_flags(unsigned char *setup,unsigned start,unsigned end) {
    flags=0;unsigned failures=0;
    for(unsigned at=start;at<end;) {
        unsigned kind=setup[at+3];
        if(kind==PROPDEF_OBJECTIVE_COMPLETE_CONDITION)flags|=word(setup+at+4);
        if(kind==PROPDEF_OBJECTIVE_FAIL_CONDITION)failures|=word(setup+at+4);
        int n=sizepropdef((PropDefHeaderRecord *)(setup+at));assert(n>0);at+=4*(unsigned)n;
    }
    assert(!(flags&failures));
}
int main(int argc,char **argv) {
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));long length=ftell(f);assert(length>=40 && length<1024*1024);rewind(f);
    unsigned char *setup=malloc((size_t)length);assert(setup && fread(setup,1,(size_t)length,f)==(size_t)length);fclose(f);
    unsigned at=word(setup+12),boundary=word(setup+8),parent=0,copy=0,photo=0,copy_parent=0,photo_parent=0,copy_end=0,photo_end=0;
    for(;at<boundary;) {
        unsigned kind=setup[at+3];
        if(kind==PROPDEF_OBJECTIVE_START)parent=at;
        if(kind==PROPDEF_OBJECTIVE_COPY_ITEM){assert(parent);copy=at;copy_parent=parent;}
        if(kind==PROPDEF_OBJECTIVE_PHOTOGRAPH){assert(parent);photo=at;photo_parent=parent;}
        if(kind==PROPDEF_OBJECTIVE_END){if(parent==copy_parent)copy_end=at;if(parent==photo_parent)photo_end=at;parent=0;}
        int n=sizepropdef((PropDefHeaderRecord *)(setup+at));assert(n>0);at+=4*(unsigned)n;
    }
    assert(at==boundary && copy && photo && copy_end && photo_end);
    assert(sizepropdef((PropDefHeaderRecord *)(setup+copy))==3);
    objective_ptrs[0]=(struct objective_entry *)(setup+copy_parent);
    prepare_flags(setup,copy_parent,copy_end);
    assert(get_status_of_objective(0)==OBJECTIVESTATUS_INCOMPLETE);
    key_copied=1;assert(get_status_of_objective(0)==OBJECTIVESTATUS_COMPLETE);
    objective_ptrs[0]=(struct objective_entry *)(setup+photo_parent);
    prepare_flags(setup,photo_parent,photo_end);photo_tag=word(setup+photo+4);photo_object.prop=&photo_prop;
    assert(word(setup+photo+8)==0 && get_status_of_objective(0)==OBJECTIVESTATUS_INCOMPLETE);
    unsigned completed=1;memcpy(setup+photo+8,&completed,4);
    assert(get_status_of_objective(0)==OBJECTIVESTATUS_COMPLETE);
    completed=0;memcpy(setup+photo+8,&completed,4);healthy=0;
    assert(get_status_of_objective(0)==OBJECTIVESTATUS_FAILED);
    free(setup);
    puts("PASS original Bunker objective evaluator: complete setup traversal, 3-word key-copy record, analyzer completion, photograph completion and destroyed-target failure");
    return 0;
}
