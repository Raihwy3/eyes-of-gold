#include <assert.h>
#include <stdio.h>
#include "native_work_profile.h"
static int enabled,records;
uint64_t native_work_profile_clock(void){return enabled?1234:0;}
void native_work_profile_record(unsigned stage,uint64_t begin,uint64_t end){assert(stage==NATIVE_WORK_MAIN && begin==1234 && end==1234);records++;}
int main(void){
    assert(!native_work_profile_begin());native_work_profile_end(NATIVE_WORK_MAIN,0);assert(!records);
    enabled=1;uint64_t start=native_work_profile_begin();native_work_profile_end(NATIVE_WORK_MAIN,start);assert(records==1);
    struct NativeWorkWindow w={0};
    assert(!native_work_window_add(&w,0,100) && !w.count);
    assert(!native_work_window_add(&w,100,99) && !w.count);
    assert(!native_work_window_add(&w,100,1100));
    assert(!native_work_window_add(&w,2000,4000));
    assert(native_work_window_add(&w,5000000,5000100));
    assert(w.begin==100 && w.total==3100 && w.peak==2000 && w.count==3);
    struct NativeWorkWindow nested={0};
    assert(!native_work_window_add(&nested,200,700));
    assert(nested.total==500 && w.total==3100); /* Nested stages remain separate. */
    puts("PASS worker timing: disabled hooks, valid intervals, exact five-second windows, total/peak/count and independent nested stages");
}
