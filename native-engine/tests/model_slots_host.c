#include <ultra64.h>
#include "objecthandler.h"
#include "model.h"
#include "initunk_005520.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
struct ModelSlot *g_ModelSlots;
struct AnimModelSlot *g_AnimModelSlots;
s32 g_MaxModelSlots,g_MaxAnimModelSlots,g_ModelIsLvResetting;
static void *allocations[256];
static unsigned allocation_count;
void *mempAllocBytesInBank(u32 bytes,u8 bank) {
    (void)bank;
    if(allocation_count==256)abort();
    void *p=calloc(1,bytes);
    if(!p)abort();
    allocations[allocation_count++]=p;
    return p;
}
void sysFatalError(const char *fmt,...) { (void)fmt;abort(); }
void osSyncPrintf(const char *fmt,...) { (void)fmt;abort(); }
int main(void) {
    ModelFileHeader a={0},b={0};
    modelmgrSetLevelResetting(1);
    modelmgrAllocateModelSlots(2);
    modelmgrAllocateAnimModelSlots(2);
    Model *m1=modelmgrInstantiateModel(&a),*m2=modelmgrInstantiateModel(&b);
    Model *a1=modelmgrInstantiateModelWithAnim(&a),*a2=modelmgrInstantiateModelWithAnim(&b);
    if(!m1||!m2||!a1||!a2||m1==m2||a1==a2)abort();
    if(m1->obj!=&a||m2->obj!=&b||a1->obj!=&a||a2->obj!=&b)abort();
    modelmgrSetLevelResetting(0);
    for(unsigned pass=0;pass<100;pass++) {
        clear_model_obj(m1);clear_model_obj(a1);
        m1=modelmgrInstantiateModel(&a);a1=modelmgrInstantiateModelWithAnim(&a);
        if(!m1||!a1||m1==m2||a1==a2||m2->obj!=&b||a2->obj!=&b)abort();
    }
    // Spare slots preserve their preallocated RW pools through free/reuse.
    a.numRecords=4;
    Model *spare=modelmgrInstantiateModel(&a);
    Model *animated_spare=modelmgrInstantiateModelWithAnim(&a);
    if(!spare||!animated_spare||!spare->datas||!animated_spare->datas)abort();
    u32 *rw=spare->datas,*anim_rw=animated_spare->datas;
    clear_model_obj(spare);clear_model_obj(animated_spare);
    spare=modelmgrInstantiateModel(&a);animated_spare=modelmgrInstantiateModelWithAnim(&a);
    if(spare->datas!=rw||animated_spare->datas!=anim_rw||m2->obj!=&b||a2->obj!=&b)abort();
    for(unsigned i=0;i<allocation_count;i++)free(allocations[i]);
    puts("PASS original ordinary/animated model allocation, distinct live slots, 100 free/reuse cycles and spare RW-pool retention");
    return 0;
}
