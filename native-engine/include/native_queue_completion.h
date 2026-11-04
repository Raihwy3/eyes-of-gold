#ifndef NATIVE_QUEUE_COMPLETION_H
#define NATIVE_QUEUE_COMPLETION_H
/* Caller holds the queue mutex. Preserve FIFO order of every retained item. */
static inline int native_queue_post_completion(OSMesgQueue *mq, OSMesg message,
                                               int has_retrace, OSMesg retrace) {
    if (mq->msgCount <= 0) return 0;
    if (mq->validCount >= mq->msgCount) {
        int drop=-1;
        if (has_retrace) for (int i=0;i<mq->validCount;i++)
            if (mq->msg[(mq->first+i)%mq->msgCount]==retrace) {drop=i;break;}
        if (drop<0) return 0;
        for (int i=drop;i+1<mq->validCount;i++)
            mq->msg[(mq->first+i)%mq->msgCount]=mq->msg[(mq->first+i+1)%mq->msgCount];
        mq->validCount--;
    }
    mq->msg[(mq->first+mq->validCount)%mq->msgCount]=message;
    mq->validCount++;
    return 1;
}
#endif
