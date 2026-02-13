#include <xt/scheduler.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/ringQueue.h>
#include <xt/list.h>

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
    if (currentThread->state == XT_THREAD_RUN_STATE) {
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
        if (currentThread->state == XT_THREAD_RUN_STATE) {
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

XTResult xtSchedulerInit() {

    XT_TRY(xtCreateThread(xtIdleThreadFunction, 0x1000, NULL, XT_THREAD_RUN_STATE, &IdleThread));
    return XT_SUCCESS;
}

#include <xt/pe.h>
#include <xt/memory.h>
#include <xt/arch/x86_64.h>
#include <xt/io.h>
#include <xt/scheduler.h>
#include <xt/random.h>
#include <xt/kernel.h>

extern void* kernelPageTable;

XTResult xtCopyMem(void* dst, void* src, uint64_t size);
XTResult xtSetMem(void* dst, uint8_t c, uint64_t size);

extern void xtUserExit();

XTResult xtCreateProcess(XTProcess** out) {

    void* newPageTable = NULL;
    XT_TRY(xtAllocatePages(NULL, 0x1000, &newPageTable));
    xtSetMem(newPageTable, 0, 0x1000);
    xtCopyMem(newPageTable, kernelPageTable, 0x1000);
    
    uint64_t stackAslr = 0;
    xtGetRandomU64(&stackAslr);
    stackAslr = (stackAslr & (((1ull << 35) - 1)) << 12);

    uint64_t moduleAslr = 0;
    xtGetRandomU64(&moduleAslr);
    moduleAslr = (0x00007f0000000000) + ((moduleAslr & ((1ull << 28) - 1)) << 12);
    xtDebugPrint("stackAslr 0x%016llx moduleAslr 0x%016llx\n", stackAslr, moduleAslr);

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

    }

    xtDebugPrint("set module!\n");

    uint64_t physStack = 0;
    XT_TRY(xtAllocatePages(NULL, 0x2000, &physStack));

    xtSetPages(newPageTable, 
        (void*)(stackAslr + (0x100000 - 0x2000)), 
        (void*)physStack, 
        0x2000, XT_MEM_USER | XT_MEM_WRITE | XT_MEM_READ);
    
    xtDebugPrint("set stack!\n");

    uint64_t returnFromThread = xtUserExit;
    //get virtual user space address of xtUserExit
    uint64_t physStackTop = physStack+0x2000;
    uint64_t* stack = (uint64_t*)(physStackTop);
    --stack;
    *stack = ((uint64_t)xtUserExit & 0xfff) + 0x00007ffffffff000;


    XTContext* context = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTContext), &context));
    xtSetMem(context, 0, sizeof(XTContext));
    context->ss = 0x23;
    context->cs = 0x1b;
    context->rcx = 0;
    context->rip = moduleAslr + ntHeaders->OptionalHeader.AddressOfEntryPoint;
    context->rsp = stackAslr + (0x100000) - 8;
    context->rflags = 0x202;
    xtDebugPrint("rsp 0x%016llx rip 0x%016llx", context->rsp, context->rip);
    
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

    result->context = context;
    result->result = 0;
    result->state = XT_THREAD_RUN_STATE;
    result->privilage = 1;
    result->ticks = 100;

    asm volatile("mov %%rax, %%cr3"::"a"(newPageTable));

    return XT_SUCCESS;

}

XTResult xtCreateThread(PFNXTTHREADFUNC ThreadFunc, size_t size, void* arg, uint8_t state, XTThread** out) {
    
    XT_CHECK_ARG_IS_NULL(ThreadFunc);
    XT_CHECK_ARG_IS_NULL(out);

    uint64_t* stack_raw;
    XT_TRY(xtAllocatePages(NULL, 0x1000, (void**)&stack_raw));
    
    // 1. Находим конец страницы (стек растет вниз)
    // Обязательно используем uintptr_t для корректной арифметики!
    uintptr_t stack_top = (uintptr_t)stack_raw + 0x1000;

    // 2. Выравнивание по 16 байт и резерв 32 байта Shadow Space 
    // (по спецификации MS ABI для вызываемой функции)
    stack_top -= 40; 

    XTContext* ctx = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTContext), &ctx));

    // Инициализируем "сохраненное" состояние
    ctx->rip = ThreadFunc;                    // Прерывания разрешены (IF=1)
    ctx->rflags = 0x202;
    ctx->rcx = arg;
    ctx->cs = 0x08;
    ctx->ss = 0x10;
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

    result->context = ctx;
    result->result = 0;
    result->state = state;
    result->privilage = 1;
    result->ticks = 100;
    *out = result;
    // Теперь нам нужна маленькая хитрость: xtThreadEntryWrapper должен 
    // знать, что вызывать. Перепишем враппер в ASM или используем регистры.
    return XT_SUCCESS;
}