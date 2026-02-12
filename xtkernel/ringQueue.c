#include <xt/ringQueue.h>
#include <xt/memory.h>

struct XTRingQueue {
    void* data;
    struct XTRingQueue* next;
};

XTResult xtPopQueue(XTRingQueue** ringQueue, void** data) {

    XT_CHECK_ARG_IS_NULL(ringQueue);
    XT_CHECK_ARG_IS_NULL(data);
    XT_CHECK_ARG_IS_NULL(*ringQueue);

    *data = (*ringQueue)->data;
    if ((*ringQueue)->next != (*ringQueue)) {
        XTRingQueue* temp = (*ringQueue);
        *ringQueue = (*ringQueue)->next;
        xtHeapFree(temp);
    }
    return XT_SUCCESS;
}

XTResult xtGetLastRingQueue(XTRingQueue* ringQueue, XTRingQueue** last) {

    XT_CHECK_ARG_IS_NULL(ringQueue);
    XT_CHECK_ARG_IS_NULL(last);

    for (;ringQueue->next && ringQueue->next != ringQueue; ringQueue = ringQueue->next);
    *last = ringQueue;

    return XT_SUCCESS;

}

XTResult xtCreateQueue(void* data, XTRingQueue** result) {

    XT_CHECK_ARG_IS_NULL(result);

    XTRingQueue* _r = NULL;
    xtHeapAlloc(sizeof(XTRingQueue), &_r);
    _r->data = data;
    _r->next = _r;
    *result = _r;
    return XT_SUCCESS;
}

XTResult xtPushQueue(XTRingQueue* ringQueue, void* next) {
    XT_CHECK_ARG_IS_NULL(ringQueue);
    XTRingQueue* last = NULL;
    xtGetLastRingQueue(ringQueue, &last);
    XTRingQueue* _next = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTRingQueue), &_next));
    _next->data = next;
    _next->next = ringQueue;
    last->next = _next;
    return XT_SUCCESS;
}
