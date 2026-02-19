#include <xt/scheduler.h>
#include <xt/memory.h>
#include <xt/linker.h>
#include <xt/kernel.h>

XTResult xtDuplicateHandle(
    XTProcess* process,
    uint64_t handle,
    uint32_t access,
    uint32_t type,
    void* desc
) {
    XTDescriptorTable* table = process->tables[((handle >> 9) & 0x7)];
    if (table == NULL) {
        XT_TRY(xtAllocatePages(NULL, 0x1000, &table));
        process->tables[((handle >> 9) & 0x7)] = HIGHER_MEM(table);
    }
    XTDescriptor* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTDescriptor), &result));
    result->access = access;
    result->type = type;
    result->desc = desc;
    table->descs[(handle) & 0x1ff] = result;
    return XT_SUCCESS;
}

XTResult xtGetHandle(
    XTProcess* process,
    uint64_t handle,
    XTDescriptor** out
) {

    XT_CHECK_ARG_IS_NULL(process);
    XT_CHECK_ARG_IS_NULL(out);

    XTDescriptorTable* table = process->tables[((handle >> 9) & 0x7)];
    *out = table->descs[(handle) & 0x1ff];

    return XT_SUCCESS;

}

#include <xt/pe.h>
#include <xt/memory.h>
#include <xt/arch/x86_64.h>
#include <xt/io.h>
#include <xt/scheduler.h>
#include <xt/random.h>
#include <xt/kernel.h>
#include <xt/string.h>

extern void* kernelPageTable;

extern XTThread* currentThread;

extern XTList* threads;
extern XTList* currentThreadIterator;

extern uint64_t threadCount;

XTResult xtTerminateThread(XTThread* thread, XTResult result) {
    XT_CHECK_ARG_IS_NULL(thread);
    xtDebugPrint("delete thread 0x%x\n", thread->id);
    thread->result = result;
    thread->state = (thread->state & ~(0xf)) | XT_THREAD_TERMINATED_STATE;
    if (thread == currentThread)
        xtSwitchToThread();
    return XT_SUCCESS;
}

extern void xtUserExit();

XTResult xtGetCurrentThread(XTThread** out) {
    XT_CHECK_ARG_IS_NULL(out);
    *out = currentThread;
}

XTResult xtGetCurrentProcess(XTProcess** out) {
    XT_CHECK_ARG_IS_NULL(out);
    *out = currentThread->process;
}

XTResult xtCreateProcess(    
    XTProcess* parentProcess,
    uint64_t flags,
    XTProcess** out
) {

    XTProcess* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTProcess), &result));

    void* newPageTable = NULL;
    XT_TRY(xtAllocatePages(NULL, 0x1000, &newPageTable));
    xtSetMem(newPageTable, 0, 0x1000);
    xtCopyMem((char*)newPageTable + 0x7f8, (char*)kernelPageTable + 0x7f8, 0x808);
    
    result->pageTable = newPageTable;

    uint64_t moduleAslr = 0;
    xtGetRandomU64(&moduleAslr);
    moduleAslr = (0x00007f0000000000) + ((moduleAslr & ((1ull << 26) - 1)) << 12);

    PFNXTMAIN main = NULL;

    XT_TRY(xtLoadModule(
        result,
        "/initrd/xtinit.xte",
        moduleAslr,
        XT_MEM_USER,
        &main
    ));

    XTThread* thread = NULL;

    XT_TRY(xtCreateThread(
        result,
        (PFNXTTHREADFUNC)(main),
        0,
        NULL,
        XT_THREAD_USER | XT_THREAD_RUN_STATE,
        &thread
    ));
    
    return XT_SUCCESS;

}

XTResult xtAllocateUserStack(
    XTProcess* process,
    uint64_t stackSize,
    void** vmOut,
    void** hmmOut
) {

    XT_CHECK_ARG_IS_NULL(vmOut);
    XT_CHECK_ARG_IS_NULL(hmmOut);

    uint64_t stack_raw = 0;
    XT_TRY(xtAllocatePages(NULL, 0x1000, (void**)&stack_raw));

    uint64_t hmm_stack_raw = HIGHER_HALF_MEM((uint64_t)stack_raw);

    uint64_t stackFlags = XT_MEM_RESERVED | XT_MEM_READ | XT_MEM_WRITE | XT_MEM_USER;
    uint64_t virtualStack = 0;
    xtGetRandomU64(&virtualStack);
    virtualStack = ((virtualStack & ((1ull << 34) - 1)) << 12);

    XT_TRY(xtInsertVirtualMap(
        process,
        virtualStack,
        NULL,
        stackSize,
        stackFlags
    ));

    XT_TRY(xtSetPages(
        process->pageTable, 
        (uint64_t)virtualStack + (stackSize - 0x1000), 
        stack_raw, 0x1000, 
        stackFlags
    ));
    
    *vmOut = (char*)virtualStack + stackSize;
    *hmmOut = (char*)hmm_stack_raw+0x1000;
    return XT_SUCCESS;
}

XTResult xtCreateThread(
    XTProcess* process, 
    PFNXTTHREADFUNC ThreadFunc, 
    size_t stackSize, 
    void* arg, 
    uint8_t state,
    XTThread** out
) {
    if (stackSize == 0) {
        stackSize = 0x100000;
    }
    
    XT_CHECK_ARG_IS_NULL(ThreadFunc);
    XT_CHECK_ARG_IS_NULL(out);

    void* kernelStack = NULL;

    XT_TRY(xtAllocatePages(NULL, 0x4000, &kernelStack));
    kernelStack = HIGHER_HALF_MEM(kernelStack);

    // 2. Выравнивание по 16 байт и резерв 32 байта Shadow Space 
    // (по спецификации MS ABI для вызываемой функции)

    uint64_t stack_top = 0;
    uint64_t physStackTop = 0;
    if (state & XT_THREAD_USER) {
        void* vmOut = NULL;
        void* hmmOut = NULL;
        XT_TRY(xtAllocateUserStack(
            process,
            stackSize,
            &vmOut,
            &hmmOut
        ));
        stack_top = (char*)vmOut;
        physStackTop = (char*)hmmOut;
    }
    else {
        stack_top = (char*)kernelStack + 0x4000;
        physStackTop = (char*)kernelStack + 0x4000;
    }
    stack_top -= 40;
    physStackTop -= 40; 
    void* ctx = NULL;
    xtSetContext(
        process, 
        kernelStack,
        ThreadFunc,
        arg,
        physStackTop,
        stack_top,
        state,
        &ctx
    );

    XTThread* result = NULL;

    if (threads == NULL) {
        XT_TRY(xtHeapAlloc(sizeof(XTThread), &result));
        XT_TRY(xtCreateList(result, &threads));
        currentThread = result;
        currentThreadIterator = threads;
        result->id = 0;
    }
    else {
        for (XTList* i = threads; i; xtGetNextList(i, &i)) {
            XTThread* thread = NULL;
            xtGetListData(i, &thread);
            if (thread->state == XT_THREAD_TERMINATED_STATE) {
                result = thread;
            }
        }
    }

    if (result == NULL) {
        XT_TRY(xtHeapAlloc(sizeof(XTThread), &result));
        XTList* newList = NULL;
        XT_TRY(xtCreateList(result, &newList));
        xtAppendList(threads, newList);
        result->id = ++threadCount;
    }
    result->process = process;
    result->context = ctx;
    result->result = 0;
    result->state = state;
    result->privilage = 1;
    result->ticks = 10;
    result->kernelStack = kernelStack+0x4000;
    XTList* threadList = NULL;
    XT_TRY(xtCreateList(result, &threadList));
    if (process->threads == NULL) {
        process->threads = threadList;
    }
    else {
        xtAppendList(process->threads, threadList);
    }

    *out = result;
    // Теперь нам нужна маленькая хитрость: xtThreadEntryWrapper должен 
    // знать, что вызывать. Перепишем враппер в ASM или используем регистры.
    return XT_SUCCESS;
}
