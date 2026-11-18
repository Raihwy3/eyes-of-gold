#ifndef EOG_NATIVE_LOG_BUFFER_H
#define EOG_NATIVE_LOG_BUFFER_H
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#define NATIVE_LOG_CAPACITY 65536u
struct NativeLogBuffer {
    pthread_mutex_t queue_lock,io_lock;
    char bytes[NATIVE_LOG_CAPACITY];
    unsigned head,size,dropped;
};
#define NATIVE_LOG_INITIALIZER {PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER,{0},0,0,0}
/* Producers hold only the memory-copy lock, never the file/SD lock. */
static inline void native_log_enqueue(struct NativeLogBuffer *q,const char *text,unsigned size) {
    pthread_mutex_lock(&q->queue_lock);
    if(size>NATIVE_LOG_CAPACITY-q->size)q->dropped++;
    else {
        unsigned tail=(q->head+q->size)%NATIVE_LOG_CAPACITY;
        unsigned first=NATIVE_LOG_CAPACITY-tail;if(first>size)first=size;
        memcpy(q->bytes+tail,text,first);memcpy(q->bytes,text+first,size-first);q->size+=size;
    }
    pthread_mutex_unlock(&q->queue_lock);
}
static inline void native_log_drain(struct NativeLogBuffer *q,FILE *file,int wait) {
    if(!file)return;
    if(wait)pthread_mutex_lock(&q->io_lock);
    else if(pthread_mutex_trylock(&q->io_lock))return;
    char block[8192];unsigned dropped=0;
    /* Bound each pass even if producers keep logging. Never hold the queue
     * lock across fwrite/fflush, including a slow or stalled filesystem. */
    for(unsigned pass=0;pass<NATIVE_LOG_CAPACITY/sizeof block;pass++) {
        pthread_mutex_lock(&q->queue_lock);
        unsigned size=q->size;if(size>sizeof block)size=sizeof block;
        unsigned first=NATIVE_LOG_CAPACITY-q->head;if(first>size)first=size;
        memcpy(block,q->bytes+q->head,first);memcpy(block+first,q->bytes,size-first);
        q->head=(q->head+size)%NATIVE_LOG_CAPACITY;q->size-=size;
        dropped+=q->dropped;q->dropped=0;
        pthread_mutex_unlock(&q->queue_lock);
        if(size)fwrite(block,1,size,file);
        if(!size)break;
    }
    if(dropped)fprintf(file,"\nLog buffer overflow: %u records dropped\n",dropped);
    fflush(file);
    pthread_mutex_unlock(&q->io_lock);
}
#endif
