#include <xt/scheduler.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/ringQueue.h>
#include <xt/list.h>
#include <xt/random.h>

XTThread* currentThread = NULL;

XTList* threads = NULL;
XTList* currentThreadIterator = NULL;

uint64_t threadCount = 0;

XTResult xtTerminateThread(XTThread* thread, XTResult result) {
    XT_CHECK_ARG_IS_NULL(thread);
    xtDebugPrint("delete thread 0x%x\n", thread->id);
    thread->result = result;
    thread->state = XT_THREAD_TERMINATED_STATE;
    xtSwitchToThread();
    return XT_SUCCESS;
}

XTResult xtMemSet(void* data, uint8_t c, uint64_t count);

extern void xtSwitchTo();

XTResult xtSleepThread(
    XTThread* thread,
    uint64_t milliseconds
) {

    XT_CHECK_ARG_IS_NULL(thread);

    thread->state = XT_THREAD_SLEEP_STATE;
    thread->ticks = milliseconds;
    xtSwitchToThread();
    return XT_SUCCESS;
}

XTResult xtWakeUpThread(XTThread* thread) {

    thread->state = XT_THREAD_RUN_STATE;
    thread->ticks = thread->privilage * 10;
    return XT_SUCCESS;

}

XTResult xtWakeUpThreads() {
    for (XTList* i = threads; i; xtGetNextList(i, &i)) {
        XTThread* thread = NULL;
        xtGetListData(i, &thread);
        if (thread->state == XT_THREAD_SLEEP_STATE) {
            thread->ticks -= 10;
            if (thread->ticks == 0) {
                xtWakeUpThread(thread);
            }
        }
    }
    return XT_SUCCESS;
}

void xtRegDump();

XTThread* IdleThread = NULL;

void xtSchedule() {
    // 1. Сначала будим все потоки, которые пора разбудить
    xtWakeUpThreads();

    // 2. Уменьшаем квант времени текущего потока
    if ((currentThread->state & 0xf) == XT_THREAD_RUN_STATE) {
        currentThread->ticks -= 10;
        // Если время потока еще не вышло и он не заснул сам, продолжаем выполнение
        if (currentThread->ticks > 0) return;
    }

    // 3. Ищем следующий поток, который ГОТОВ к выполнению (RUN_STATE)
    XTList* startThreadIter = currentThreadIterator;
    
    while (1) {
        // Переходим к следующему
        
        xtGetNextList(currentThreadIterator, &currentThreadIterator);
        if (currentThreadIterator == NULL) {
            currentThreadIterator = threads;
        }
        xtGetListData(currentThreadIterator, &currentThread);
        // Если нашли поток, который готов бежать — выходим из цикла поиска
        if ((currentThread->state & 0xf) == XT_THREAD_RUN_STATE) {
            break;
        }

        // Защита от бесконечного цикла: если обошли все и никто не готов
        if (currentThreadIterator == startThreadIter) {
            // В идеале тут нужно переходить в Idle-поток (ожидание прерывания)
            // Но для начала просто выйдем
            currentThread = IdleThread;
            break;
        }
    }

    // Сбрасываем квант времени для выбранного потока
    currentThread->ticks = currentThread->privilage * 100;
}

void xtHalt();

void xtIdleThreadFunction() {
    while(1) xtHalt();
}

extern void* kernelPageTable;

XTResult xtVirtualAlloc(XTProcess* process, void* start, uint64_t size, uint64_t attr, void** out) {
    if ((uint64_t)start > USER_SIZE_MAX) return XT_OUT_OF_MEMORY;
    if (start == NULL) {
        uint64_t aslr = 0;
        xtGetRandomU64(&aslr);
        start = aslr & (((1ull << 35) - 1)) << 12;
        xtDebugPrint("start 0x%llx\nASLR 0x%llx\n", start, aslr);
    }
    XTList* memoryMapEntry = NULL;
    XTVirtualMap* virtualMap = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTVirtualMap), &virtualMap));
    virtualMap->attr = attr;
    virtualMap->physicalAddress = 0;
    virtualMap->virtualStart = start;
    virtualMap->size = size;
    xtCreateList(virtualMap, &memoryMapEntry);
    if (process->memoryMap == NULL) {
        process->memoryMap = memoryMapEntry;
    }
    else {
        xtAppendList(process->memoryMap, memoryMapEntry);
    }
    *out = start;
    return XT_SUCCESS;
}

XTResult xtVirtualFree(XTProcess* process, void* start, uint64_t size) {
    for (XTList* l = process->memoryMap; l; xtGetNextList(l, &l)) {
        XTVirtualMap* map = NULL;
        xtGetListData(l, &map);
        if (map->virtualStart != start) continue;
        xtRemoveFromList(process->memoryMap, l);
        start = (uint64_t)start + map->size;
        size -= map->size;
    }
    return XT_SUCCESS;
}

XTProcess kernelProcess = {
    0
};


XTResult xtSchedulerInit() {
    kernelProcess.pageTable = kernelPageTable;

    XT_TRY(xtCreateThread(&kernelProcess, xtIdleThreadFunction, 0, NULL, XT_THREAD_RUN_STATE, &IdleThread));
    return XT_SUCCESS;
}

#include <xt/pe.h>
#include <xt/memory.h>
#include <xt/arch/x86_64.h>
#include <xt/io.h>
#include <xt/scheduler.h>
#include <xt/random.h>
#include <xt/kernel.h>



XTResult xtCopyMem(void* dst, void* src, uint64_t size);
XTResult xtSetMem(void* dst, uint8_t c, uint64_t size);

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
    xtCopyMem((char*)newPageTable + 0x800, (char*)kernelPageTable + 0x800, 0x800);
    
    result->pageTable = newPageTable;
    xtSetPages(
        newPageTable, 
        (void*)(NULL), 
        (void*)(NULL), 
        0x8000, XT_MEM_READ | XT_MEM_USER);
    void* beginOfProcess = NULL;
    //xtVirtualAlloc(result, NULL, 0x8000, XT_MEM_READ | XT_MEM_USER | XT_MEM_RESERVED, &beginOfProcess);

    uint64_t moduleAslr = 0;
    xtGetRandomU64(&moduleAslr);
    moduleAslr = (0x00007f0000000000) + ((moduleAslr & ((1ull << 28) - 1)) << 12);

    PIMAGE_DOS_HEADER dosHeader = userProgram;
    PIMAGE_NT_HEADERS ntHeaders = (char*)userProgram + dosHeader->e_lfanew;
    void* physImage = 0;
    XT_TRY(xtAllocatePages(NULL, ntHeaders->OptionalHeader.SizeOfImage, &physImage));
    xtCopyMem(physImage, userProgram, ntHeaders->OptionalHeader.SizeOfHeaders);

    PIMAGE_SECTION_HEADER sections = (char*)ntHeaders + IMAGE_SIZEOF_NT_OPTIONAL64_HEADER + IMAGE_SIZEOF_FILE_HEADER + 4;
    for (uint64_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        xtCopyMem(physImage + sections[i].VirtualAddress, 
            (char*)userProgram + sections[i].PointerToRawData, sections[i].SizeOfRawData);
        
        uint64_t attr = XT_MEM_USER;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            attr |= XT_MEM_EXEC;
        }
        if (sections[i].Characteristics & IMAGE_SCN_MEM_READ) {
            attr |= XT_MEM_READ;
        }
        if (sections[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            attr |= XT_MEM_WRITE;
        }
        xtSetPages(newPageTable, 
            moduleAslr + sections[i].VirtualAddress, 
            physImage + sections[i].VirtualAddress, (sections[i].SizeOfRawData+0xfff) & (~(0xfff)), attr);
        
        void* dummy = NULL;

        xtVirtualAlloc(result, 
            moduleAslr + sections[i].VirtualAddress, 
            (sections[i].SizeOfRawData+0xfff) & (~(0xfff)), attr, &dummy);

    }

    XTThread* thread = NULL;

    xtCreateThread(
        result,
        (PFNXTTHREADFUNC)(moduleAslr + ntHeaders->OptionalHeader.AddressOfEntryPoint),
        0,
        NULL,
        XT_THREAD_USER | XT_THREAD_RUN_STATE,
        &thread
    );

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

    uint64_t* stack_raw;
    XT_TRY(xtAllocatePages(NULL, 0x1000, (void**)&stack_raw));

    uint64_t stackFlags = XT_MEM_RESERVED | XT_MEM_READ | XT_MEM_WRITE;
    if (state & XT_THREAD_USER) {
        stackFlags |= XT_MEM_USER;
    }
    void* virtualStack = 0;
    XT_TRY(xtVirtualAlloc(process, NULL, stackSize, stackFlags, &virtualStack));
    
    xtDebugPrint("virtualStack 0x%llx\n", virtualStack);

    XT_TRY(xtSetPages(
        process->pageTable, 
        (uint64_t)virtualStack + (stackSize - 0x1000), 
        stack_raw, 0x1000, 
        stackFlags
    ));

    // 1. Находим конец страницы (стек растет вниз)
    // Обязательно используем uintptr_t для корректной арифметики!
    uintptr_t stack_top = (uintptr_t)virtualStack + stackSize;

    // 2. Выравнивание по 16 байт и резерв 32 байта Shadow Space 
    // (по спецификации MS ABI для вызываемой функции)

    uint64_t* ret = stack_raw + 0x1000 - 40;
    *ret = xtUserExit;

    stack_top -= 40; 

    XTContext* ctx = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTContext), &ctx));

    // Инициализируем "сохраненное" состояние
    ctx->rip = ThreadFunc;                    // Прерывания разрешены (IF=1)
    ctx->rflags = 0x202;
    ctx->rcx = arg;
    if (state & XT_THREAD_USER) {
        ctx->cs = 0x1b;
        ctx->ss = 0x23;
    }
    else {
        ctx->cs = 0x08;
        ctx->ss = 0x10;
    }

    ctx->rsp = stack_top;

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
    xtDebugPrint("Thread 0x%llx process 0x%llx\n", result, process);
    result->process = process;
    result->context = ctx;
    result->result = 0;
    result->state = state;
    result->privilage = 1;
    result->ticks = 100;

    XTList* threadList = NULL;
    xtCreateList(result, threadList);
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