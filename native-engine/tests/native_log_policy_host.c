#include "native_log_policy.h"
#include "native_log_buffer.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
static FILE *log_file;
static struct NativeLogBuffer log_queue=NATIVE_LOG_INITIALIZER;
#include "native-log-functions.h"
int main(void) {
    char directory[]="/tmp/eog-log-policy-XXXXXX";
    assert(mkdtemp(directory));
    char flag[512],path[512];
    snprintf(flag,sizeof flag,"%s/debug-log.flag",directory);
    snprintf(path,sizeof path,"%s/native-game.log",directory);
    assert(!native_log_open(directory) && access(path,F_OK)!=0);
    FILE *old=fopen(path,"w");assert(old);fputs("old log",old);fclose(old);
    assert(!native_log_open(directory));
    int formatted=123;
    osSyncPrintf("disabled%n",&formatted);
    sysLogPrintf(0,"disabled%n",&formatted);
    assert(formatted==123 && log_queue.size==0);
    old=fopen(path,"r");assert(old);char contents[128]={0};
    assert(fread(contents,1,sizeof contents-1,old)==7 && !strcmp(contents,"old log"));fclose(old);
    assert(mkdir(flag,0700)==0 && !native_log_open(directory));assert(rmdir(flag)==0);
    FILE *enable=fopen(flag,"w");assert(enable);fclose(enable);
    log_file=native_log_open(directory);assert(log_file);
    osSyncPrintf("enabled ");sysLogPrintf(0,"message %d",42);
    fclose(log_file);log_file=NULL;
    old=fopen(path,"r");assert(old);memset(contents,0,sizeof contents);
    assert(fread(contents,1,sizeof contents-1,old)>0 && !strcmp(contents,"enabled message 42\n"));fclose(old);
    assert(unlink(flag)==0 && !native_log_open(directory));
    /* A write failure also leaves logging disabled. */
    assert(unlink(path)==0 && mkdir(path,0700)==0);
    enable=fopen(flag,"w");assert(enable);fclose(enable);
    assert(!native_log_open(directory));
    assert(unlink(flag)==0 && rmdir(path)==0 && rmdir(directory)==0);
    puts("Log policy passed: opt-in only, old log preserved, no disabled formatting/queueing, write failure safe");
    return 0;
}
