#ifndef __XT_SCHEDULER_H__
#define __XT_SCHEDULER_H__

#include <xt/list.h>
#include <xt/io.h>
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
    uint64_t offset;
    XTFile* file;
} XTVirtualMap;

// Новые определения для дескрипторных таблиц
#define HANDLE_LEVEL_BITS 9
#define HANDLE_ENTRIES_PER_TABLE (1 << HANDLE_LEVEL_BITS)   // 512
#define HANDLE_LEVEL_MASK (HANDLE_ENTRIES_PER_TABLE - 1)   // 0x1FF
#define HANDLE_LEVELS 4


#define XT_DESCRIPTOR_TYPE_UNDEFINED 0
#define XT_DESCRIPTOR_TYPE_PROCESS   1
#define XT_DESCRIPTOR_TYPE_FILE      2
#define XT_DESCRIPTOR_TYPE_DIRECTORY 3
#define XT_DESCRIPTOR_TYPE_THREAD    4

typedef struct XTDescriptorTable {
    XTDescriptor* entries[HANDLE_ENTRIES_PER_TABLE];   // указатели на подтаблицы или дескрипторы
} XTDescriptorTable;

// Обновлённая структура процесса
typedef struct XTProcess {
    XTList* threads;
    void*   pageTable;
    struct XTProcess* parentProcess;
    XTList* childProcess;
    XTList* memoryMap;
    XTDescriptorTable* descriptorRoot;   // корневая таблица дескрипторов
    uint64_t flags;
    uint64_t activeThreads;               // счётчик живых потоков
} XTProcess;

// Новый флаг для приватных страниц (освобождаемых при завершении)
#define XT_MEM_PRIVATE 0x1000

XTResult xtSetContext(
    XTProcess* process, 
    void* kernelStack,
    uint64_t function, 
    uint64_t arg,
    uint64_t physStackTop,
    uint64_t stackTop, 
    uint64_t flags,
    void** context
);


XTResult xtDuplicateHandle(
    XTProcess* process,
    uint64_t position,
    uint32_t access,
    uint32_t type,
    void* desc
);

XTResult xtGetHandle(
    XTProcess* process,
    uint64_t handle,
    XTDescriptor** out
);


XTResult xtCreateVirtualMapEntry(
    void* virtualStart,
    void* physicalStart,
    uint64_t size,
    uint64_t attr,
    XTVirtualMap** out
);

XTResult xtInsertVirtualMap(
    XTProcess* process,
    void* virtualStart,
    void* physicalAddress,
    uint64_t size,
    uint64_t attr
);

XTResult xtFindVirtualMap(
    XTProcess* process,
    void* ptr,
    XTVirtualMap** out,
    XTList** prev
);

typedef struct XTThread {
    void* context; //0x0
    XTResult result; //8
    uint32_t id; //16
    uint8_t state; //20
    uint8_t privilage; //21
    uint16_t flags; //22
    uint64_t ticks; //24
    XTProcess* process;//32
    void* kernelStack; //40
} XTThread;

typedef struct XTPerCPUData {
    XTThread* currentThread;
} XTPerCPUData;

#define XT_THREAD_SLEEP_STATE      0
#define XT_THREAD_RUN_STATE        1
#define XT_THREAD_LOADED_STATE     2
#define XT_THREAD_TERMINATED_STATE 3
#define XT_THREAD_WAIT_STATE       4

#define XT_THREAD_USER             0x80
#define XT_PROCESS_TERMINATING     1


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

XTResult xtTerminateProcess(
    XTProcess* process,
    XTResult result
);


XTResult xtSleepThread(
    XTThread* thread,
    uint64_t milliseconds
);

XTResult xtSetThreadPriority(
    XTThread* thread,
    uint64_t priority
);

XTResult xtGetCurrentThread(XTThread** out);
XTResult xtGetCurrentProcess(XTProcess** out);
XTResult xtSetCurrentThread(XTThread* thread);

XTResult xtCreateProcess(
    XTProcess* parentProcess,
    uint64_t flags,
    XTProcess** out
);


#define XT_SHUTDOWN_POWER_OFF 0
#define XT_SHUTDOWN_REBOOT    1

XTResult xtShutdown(uint64_t state);

#endif