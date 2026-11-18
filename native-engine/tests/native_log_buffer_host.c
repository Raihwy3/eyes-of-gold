#define _GNU_SOURCE
#include "native_log_buffer.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
static struct NativeLogBuffer queue=NATIVE_LOG_INITIALIZER;
static pthread_mutex_t sink_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sink_ready=PTHREAD_COND_INITIALIZER;
static int blocked,released;
static char output[32];static unsigned used;
static ssize_t slow_write(void *unused,const char *data,size_t size) {
    (void)unused;pthread_mutex_lock(&sink_lock);
    blocked=1;pthread_cond_broadcast(&sink_ready);
    while(!released)pthread_cond_wait(&sink_ready,&sink_lock);
    assert(used+size<=sizeof output);memcpy(output+used,data,size);used+=size;
    pthread_mutex_unlock(&sink_lock);return size;
}
static void *writer(void *file){native_log_drain(&queue,file,1);return NULL;}
int main(void) {
    struct NativeLogBuffer q=NATIVE_LOG_INITIALIZER;
    FILE *file=tmpfile();assert(file);
    char a[50000],b[40000];memset(a,'a',sizeof a);memset(b,'b',sizeof b);
    native_log_enqueue(&q,a,sizeof a);native_log_drain(&q,file,1);
    native_log_enqueue(&q,b,sizeof b);native_log_drain(&q,file,1);
    rewind(file);for(unsigned i=0;i<sizeof a;i++)assert(fgetc(file)=='a');
    for(unsigned i=0;i<sizeof b;i++)assert(fgetc(file)=='b');
    assert(fgetc(file)==EOF);fclose(file);
    file=tmpfile();assert(file);
    native_log_enqueue(&q,a,sizeof a);native_log_enqueue(&q,b,sizeof b);
    assert(q.dropped==1 && q.size==sizeof a);
    native_log_drain(&q,file,1);assert(!q.size && !q.dropped);
    fseek(file,sizeof a,SEEK_SET);char line[100]={0};assert(fread(line,1,sizeof line-1,file)>0);
    assert(strstr(line,"1 records dropped"));fclose(file);
    cookie_io_functions_t funcs={.write=slow_write};file=fopencookie(NULL,"w",funcs);assert(file);
    setvbuf(file,NULL,_IONBF,0);native_log_enqueue(&queue,"first",5);
    pthread_t worker;assert(!pthread_create(&worker,NULL,writer,file));
    pthread_mutex_lock(&sink_lock);while(!blocked)pthread_cond_wait(&sink_ready,&sink_lock);pthread_mutex_unlock(&sink_lock);
    // This must return while the sink is still blocked: no queue lock is
    // held during filesystem IO. A regression is caught by the runner timeout.
    native_log_enqueue(&queue,"next",4);assert(queue.size==4);
    pthread_mutex_lock(&sink_lock);released=1;pthread_cond_broadcast(&sink_ready);pthread_mutex_unlock(&sink_lock);
    pthread_join(worker,NULL);assert(used==9 && !memcmp(output,"firstnext",9));fclose(file);
    puts("PASS queued logging: FIFO/wrap, bounded overflow, and producers remain live while filesystem writes are blocked");
}
