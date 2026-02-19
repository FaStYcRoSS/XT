#ifndef __XT_RING_QUEUE_H__
#define __XT_RING_QUEUE_H__

#include <xt/result.h>

typedef struct XTQueue XTQueue;

XTResult xtPopQueue(XTQueue* ringQueue, void** data);
XTResult xtPushQueue(XTQueue* ringQueue, void* next);
XTResult xtCreateQueue(XTQueue** result);
XTResult xtDestroyQueue(XTQueue* result);
XTResult xtRemoveFromQueue(XTQueue* q, void* data);

#endif //!__XT_RING_QUEUE_H__