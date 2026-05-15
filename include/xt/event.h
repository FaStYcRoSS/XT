#ifndef __XT_EVENT_H__
#define __XT_EVENT_H__

#include <xt/spinlock.h>
#include <xt/list.h>
#include <stdbool.h>

typedef struct XTEvent {
    XTList* list;
    XTSpinlock lock;
    bool isSignaled;
} XTEvent;

#define XT_WAIT_FOR_ALL 1

XTResult
xtCreateEvent(XTEvent** event);
XTResult
xtSignalEvent(XTEvent* event);
XTResult
xtResetEvent(XTEvent* event);
XTResult
xtWaitForEvents(
    XTEvent** events,
    uint64_t count,
    uint64_t flags,
    uint64_t milliseconds,
    uint64_t *index
);


#endif