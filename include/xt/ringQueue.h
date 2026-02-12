#ifndef __XT_RING_QUEUE_H__
#define __XT_RING_QUEUE_H__

#include <xt/result.h>

typedef struct XTRingQueue XTRingQueue;

XTResult xtPopQueue(XTRingQueue** ringQueue, void** data);
XTResult xtPushQueue(XTRingQueue* ringQueue, void* next);
XTResult xtCreateQueue(void* data, XTRingQueue** result);

#endif //!__XT_RING_QUEUE_H__