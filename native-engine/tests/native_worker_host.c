/* Exercise the actual native render-creation helper with a recorded SDK call. */
#include "native_console.h"
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
typedef int32_t Result;
typedef uint32_t u32;
#define R_FAILED(r) ((r)<0)
typedef void *Thread;
static int console_model,creation_allowed,attempts,entry_calls,last_core,last_priority;
static unsigned allowance_max,allowance_requests[3],allowance_calls,reported;
static Result APT_SetAppCpuTimeLimit(u32 percent){
 assert(allowance_calls<3);allowance_requests[allowance_calls++]=percent;
 if(percent>allowance_max)return -1;
 reported=percent;return 0;
}
static Result APT_GetAppCpuTimeLimit(u32 *percent){*percent=reported;return 0;}
static int svcGetProcessorID(void){return last_core;}
static void (*created_entry)(void *);
static void *created_arg;
static Thread threadCreate(void (*entry)(void *),void *arg,size_t stack,int priority,int core,bool detached) {
 assert(stack==128*1024 && (priority==63 || priority==48) && (core==0 || core==1 || core==2) && detached);
 last_core=core;last_priority=priority;
 attempts++;created_entry=entry;created_arg=arg;
 return creation_allowed?(Thread)&entry_calls:NULL;
}
static void sysLogPrintf(int level,const char *fmt,...) {(void)level;(void)fmt;}
#include "native-worker-creation.h"
static void *worker(void *arg){assert(arg==&entry_calls);entry_calls++;return NULL;}
int main(void) {
 for(console_model=-1;console_model<=5;console_model++) {
  attempts=entry_calls=0;creation_allowed=0;
  assert(!native_platform_start_render(worker,&entry_calls,128*1024));
  assert(attempts==native_console_is_new(console_model));
  creation_allowed=1;
  int started=native_platform_start_render(worker,&entry_calls,128*1024);
  assert(started==native_console_is_new(console_model));
  if(started){assert(last_core==2 && last_priority==63);created_entry(created_arg);assert(entry_calls==1);}
 }
 for(console_model=-1;console_model<=5;console_model++)for(unsigned cap=0;cap<4;cap++) {
  allowance_max=cap==0?0:cap==1?30:cap==2?50:89;
  for(creation_allowed=0;creation_allowed<2;creation_allowed++) {
   attempts=entry_calls=allowance_calls=reported=0;
   int started=native_platform_start_audio(worker,&entry_calls,128*1024);
   int newer=native_console_is_new(console_model);
   assert(started==((newer || allowance_max>0) && creation_allowed));
   assert(attempts==(newer || allowance_max>0));
   if(newer) {
    assert(!allowance_calls && !reported); /* No syscore request to redirect. */
   } else {
    assert(allowance_requests[0]==89);
    assert(allowance_calls==(cap<=1?3:cap==2?2:1));
    if(allowance_calls>1)assert(allowance_requests[1]==50);
    if(allowance_calls>2)assert(allowance_requests[2]==30);
   }
   if(started){assert(last_core==(newer?0:1) && last_priority==48);created_entry(created_arg);assert(entry_calls==1 && (newer || reported==allowance_max));}
  }
 }
 puts("PASS actual render creation helper: New-only core 2, retained stack/priority, trampoline argument, failure fallback and old-model bypass");
 puts("PASS actual audio creation helper: New models explicitly core 0 without APT syscore negotiation; old/unknown models retain core 1 and 89/50/30 negotiation; priority 0x30 above game/render workers, original entry/argument/stack and failure fallback");
}
