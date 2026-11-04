#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#undef NULL
#define NULL 0
typedef uintptr_t OSMesg;
typedef int s32;
typedef int OSId;
typedef struct {OSMesg *msg;int first,validCount,msgCount;} OSMesgQueue;
typedef struct {OSMesgQueue *os;pthread_mutex_t lock;pthread_cond_t cond;int loggedFirstBlock;} PortQueue;
#define PORT_MAX_QUEUES 64
#define OS_MESG_BLOCK 1
#define OS_MESG_NOBLOCK 0
static PortQueue g_pq[PORT_MAX_QUEUES];static int g_pqCount;
static pthread_mutex_t s_pqLock=PTHREAD_MUTEX_INITIALIZER;
static OSMesgQueue *g_viRetraceMQ;static OSMesg g_viRetraceMsg=1;
typedef struct {OSMesgQueue *mq;OSMesg msg;int used;} PortEvent;
static PortEvent g_events[16];
static void sysFatalError(const char *fmt,...) { (void)fmt;abort(); }
#include "native_queue_completion.h"
/* PRODUCTION_FUNCTIONS */
static OSMesgQueue work;static unsigned seen[40000];
static void *producer(void *arg){
 uintptr_t base=(uintptr_t)arg;
 for(unsigned i=0;i<10000;i++)assert(osSendMesg(&work,base+i+10,OS_MESG_BLOCK)==0);
 return NULL;
}
static void *consumer(void *arg){
 (void)arg;
 for(unsigned i=0;i<10000;i++){
  OSMesg m;assert(osRecvMesg(&work,&m,OS_MESG_BLOCK)==0);
  assert(m>=10 && m<40010);assert(__atomic_fetch_add(&seen[m-10],1,__ATOMIC_RELAXED)==0);
 }
 return NULL;
}
int main(void){
 /* Every circular start and every retrace position; critical events retain order. */
 for(int first=0;first<8;first++)for(int drop=0;drop<8;drop++){
  OSMesg storage[8];OSMesgQueue q={storage,first,8,8};
  for(int i=0;i<8;i++)storage[(first+i)%8]=i==drop?1:(OSMesg)(10+i);
  assert(native_queue_post_completion(&q,99,1,1));assert(q.validCount==8);
  int n=0;for(int i=0;i<8;i++)if(i!=drop)assert(storage[(first+n++)%8]==(OSMesg)(10+i));
  assert(storage[(first+7)%8]==99);
 }
 OSMesg full[2]={20,30};OSMesgQueue q={full,0,2,2};
 assert(!native_queue_post_completion(&q,99,1,1));assert(full[0]==20 && full[1]==30 && q.validCount==2);
 assert(!native_queue_post_completion(&q,99,0,20));
 OSMesg storage[2];osCreateMesgQueue(&work,storage,2);
 assert(osRecvMesg(&work,NULL,OS_MESG_NOBLOCK)==-1);
 osEnqueueMesg(&work,77);assert(osMesgQueueLast(&work)==77);assert(osDequeueMesg(&work)==77);
 g_viRetraceMQ=&work;g_events[0]=(PortEvent){&work,99,1};
 assert(!osSendMesg(&work,1,0));assert(!osSendMesg(&work,20,0));
 portPostEventForce(0);assert(osDequeueMesg(&work)==20);assert(osDequeueMesg(&work)==99);
 pthread_t producers[4],consumers[4];
 for(uintptr_t i=0;i<4;i++){assert(!pthread_create(&producers[i],NULL,producer,(void *)(i*10000)));assert(!pthread_create(&consumers[i],NULL,consumer,NULL));}
 for(int i=0;i<4;i++){pthread_join(producers[i],NULL);pthread_join(consumers[i],NULL);}
 for(int i=0;i<40000;i++)assert(seen[i]==1);
 puts("PASS actual OS queues: 4 producers/4 consumers, 40000 messages, FIFO completion insertion, wrapped queues and full-queue preservation");
}
