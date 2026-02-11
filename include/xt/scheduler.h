#ifndef __XT_SCHEDULER_H__
#define __XT_SCHEDULER_H__

#include <xt/list.h>

#ifdef __x86_64__
#include <xt/arch/x86_64.h>
#endif

XTResult xtSwitchToThread(void);

typedef XTResult(*PFNXTTHREADFUNC)(void);

typedef struct XTThread {
    XTContext* context;
    XTResult result;
} XTThread;

XTResult 
xtCreateThread(
    PFNXTTHREADFUNC ThreadFunc, 
    uint64_t size, 
    void* arg, 
    XTThread** out
);

XTResult xtTerminateThread(
    XTThread* thread
);
#endif