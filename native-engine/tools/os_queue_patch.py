"""Keep completion posting atomic and wake both queue predicates safely."""
def patch_os_queues(text):
    # One condition serves both not-empty and not-full. A single signal can
    # select a waiter for the wrong predicate; each waiter still rechecks it.
    text=text.replace("pthread_cond_signal(&pq->cond);","pthread_cond_broadcast(&pq->cond);")
    start=text.index("static void portPostEventForce(OSId e)\n{")
    end=text.index("\nstatic void portPostEvent(OSId e)",start)
    text=text[:start]+"""#include \"native_queue_completion.h\"
static void portPostEventForce(OSId e)
{
    PortEvent *ev = &g_events[e];
    if (!ev->used || !ev->mq) return;
    PortQueue *pq = portQueueGet(ev->mq);
    pthread_mutex_lock(&pq->lock);
    int posted = native_queue_post_completion(ev->mq, ev->msg,
        ev->mq == g_viRetraceMQ, g_viRetraceMsg);
    if (posted) pthread_cond_broadcast(&pq->cond);
    pthread_mutex_unlock(&pq->lock);
    /* Never wait on the scheduler's own full queue or discard another
     * completion. A queue containing no disposable retrace is a real fault. */
    if (!posted) sysFatalError("Completion queue full without a retrace: event=%d mq=%p", (int)e, (void *)ev->mq);
}
"""+text[end:]
    return text
