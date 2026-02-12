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
    uint32_t id;
    uint8_t state;
    uint8_t privilage;
    uint16_t flags;
    uint64_t ticks; // ms
} XTThread;

#define XT_THREAD_SLEEP_STATE      0
#define XT_THREAD_RUN_STATE        1
#define XT_THREAD_LOADED_STATE     2
#define XT_THREAD_TERMINATED_STATE 3

XTResult 
xtCreateThread(
    PFNXTTHREADFUNC ThreadFunc, 
    uint64_t size, 
    void* arg,
    uint8_t state, 
    XTThread** out
);

XTResult xtTerminateThread(
    XTThread* thread,
    XTResult result
);

XTResult xtSleepThread(
    XTThread* thread,
    uint64_t milliseconds
);

#endif