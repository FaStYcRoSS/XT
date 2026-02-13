#ifndef __XT_SCHEDULER_H__
#define __XT_SCHEDULER_H__

#include <xt/list.h>

#ifdef __x86_64__
#include <xt/arch/x86_64.h>
#endif

XTResult xtSwitchToThread(void);

typedef XTResult(*PFNXTTHREADFUNC)(void);

typedef struct XTVirtualMap {
    void* virtualStart;
    uint64_t size;
    uint64_t attr;
    void* physicalAddress;
} XTVirtualMap;

typedef struct XTProcess {
    XTList* threads;
    void*   pageTable;
    struct XTProcess* parentProcess;
    XTList* memoryMap;
    XTList* handles;
} XTProcess;

XTResult xtVirtualAlloc(XTProcess* process, void* start, uint64_t size, uint64_t attr, void** out);
XTResult xtVirtualFree(XTProcess* process, void* start, uint64_t size);

typedef struct XTThread {
    XTContext* context; //0x0
    XTResult result; //8
    uint32_t id; //16
    uint8_t state; //20
    uint8_t privilage; //21
    uint16_t flags; //22
    uint64_t ticks; //24
    XTProcess* process;//32
} XTThread;


#define XT_THREAD_SLEEP_STATE      0
#define XT_THREAD_RUN_STATE        1
#define XT_THREAD_LOADED_STATE     2
#define XT_THREAD_TERMINATED_STATE 3

#define XT_THREAD_USER             0x80

XTResult 
xtCreateThread(
    XTProcess* process,
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

XTResult xtGetCurrentThread(XTThread** out);
XTResult xtGetCurrentProcess(XTProcess** out);

XTResult xtCreateProcess(
    XTProcess* parentProcess,
    uint64_t flags,
    XTProcess** out
);

#endif