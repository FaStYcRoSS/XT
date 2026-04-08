
#include <xt/pe.h>
#include <xt/memory.h>
#include <xt/arch/x86_64.h>
#include <xt/io.h>
#include <xt/scheduler.h>
#include <xt/random.h>
#include <xt/kernel.h>
#include <xt/string.h>
#include <xt/queue.h>


static void xtFreeDescriptorTree(void* node, int level) {
    if (!node) return;
    if (level == 0) {
        XTDescriptor* desc = (XTDescriptor*)node;
        // При необходимости здесь можно вызвать деструктор объекта desc->desc
        xtHeapFree(desc);
    } else {
        XTDescriptorTable* table = (XTDescriptorTable*)node;
        for (int i = 0; i < HANDLE_ENTRIES_PER_TABLE; i++) {
            if (table->entries[i]) {
                xtFreeDescriptorTree(table->entries[i], level - 1);
            }
        }
        xtHeapFree(table);
    }
}

int64_t sign_ext(int64_t value, uint64_t bits) {
    /* generate the sign bit mask. 'b' is the extracted number of bits */
    int64_t m = 1U << (bits - 1);  

    /* Transform a 'b' bits unsigned number 'x' into a signed number 'r' */
    return ((value ^ m) - m); 
}
static void** xtResolveHandle(XTProcess* process, uint64_t handle, int create) {
    if (!process) return NULL;

    // Извлекаем индексы для каждого уровня (от старших бит к младшим)
    uint64_t indices[HANDLE_LEVELS];
    for (int i = 0; i < HANDLE_LEVELS; i++) {
        indices[i] = (handle >> (i * HANDLE_LEVEL_BITS)) & HANDLE_LEVEL_MASK;
    }

    XTDescriptorTable** current = &process->descriptorRoot;

    // Проходим уровни от верхнего (HANDLE_LEVELS-1) до первого (1)
    for (int level = HANDLE_LEVELS - 1; level > 0; level--) {
        if (*current == NULL) {
            if (!create) return NULL;
            XTDescriptorTable* newTable = NULL;
            if (xtHeapAlloc(sizeof(XTDescriptorTable), (void**)&newTable) != XT_SUCCESS)
                return NULL;
            xtMemSet(newTable, 0, sizeof(XTDescriptorTable));
            *current = newTable;
        }
        current = (XTDescriptorTable**)&((*current)->entries[indices[level]]);
    }

    // Теперь current указывает на запись в таблице последнего уровня (L0)
    return (void**)current;
}

XTResult xtGetHandle(XTProcess* process, uint64_t handle, XTDescriptor** out) {
    if (handle != sign_ext(handle, 35)) {
        return XT_INVALID_PARAMETER;
    }
    XT_CHECK_ARG_IS_NULL(process);
    XT_CHECK_ARG_IS_NULL(out);
    void** entry = xtResolveHandle(process, handle, 0);
    if (!entry) return XT_NOT_FOUND;
    *out = (XTDescriptor*)*entry;
    return (*out) ? XT_SUCCESS : XT_NOT_FOUND;
}

XTResult xtDuplicateHandle(XTProcess* process, uint64_t handle,
                           uint32_t access, uint32_t type, void* desc) {
    if (handle != sign_ext(handle, 35)) {
        return XT_INVALID_PARAMETER;
    }
    XT_CHECK_ARG_IS_NULL(process);
    void** entry = xtResolveHandle(process, handle, 1);
    if (!entry) return XT_OUT_OF_MEMORY;
    if (*entry != NULL) return XT_FILE_ALREADY_EXISTS;  // или разрешить перезапись

    XTDescriptor* newDesc = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTDescriptor), (void**)&newDesc));
    newDesc->access = access;
    newDesc->type = type;
    newDesc->desc = desc;
    *entry = newDesc;
    return XT_SUCCESS;
}

extern void* kernelPageTable;

extern XTList* threads;

extern XTList* currentThreadIterator;


extern uint64_t threadCount;

extern XTQueue* runQueue;

XTResult xtFindThreadList(XTThread* thread, XTList** out) {
    XT_CHECK_ARG_IS_NULL(thread);
    XT_CHECK_ARG_IS_NULL(out);

    for (XTList* i = thread->process->threads; i; xtGetNextList(i, &i)) {
        XTThread* threadi = NULL;
        xtGetListData(i, &threadi);
        if (threadi == thread) {
            *out = i;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;

}

XTResult xtTerminateThread(XTThread* thread, XTResult code) {
    XT_CHECK_ARG_IS_NULL(thread);
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);

    // Удаляем поток из глобального списка планировщика
    XTList* threadIter = NULL;
    xtFindThreadList(thread, &threadIter);
    xtRemoveFromList(threads, threadIter);

    // Помечаем поток завершённым
    thread->state = (thread->state & ~0x7f) | XT_THREAD_TERMINATED_STATE;
    thread->result = code;

    // Уменьшаем счётчик активных потоков процесса
    if (thread->process) {
        thread->process->activeThreads--;
        // Если это был последний поток, завершаем процесс
        if (thread->process->activeThreads == 0) {
            // Проверим, не завершается ли процесс уже (флаг)
            if (!(thread->process->flags & XT_PROCESS_TERMINATING)) {
                xtTerminateProcess(thread->process, code);
            }
        }
    }

    // Если завершаем текущий поток – переключаемся
    if (thread == currentThread) {
        xtSwitchToThread();  // не возвращается
    }
    return XT_SUCCESS;
}


XTList* processess = NULL;

uint64_t lastProcessId = 0;

XTResult xtTerminateProcess(XTProcess* process, XTResult code) {
    XT_CHECK_ARG_IS_NULL(process);
    if (process->flags & XT_PROCESS_TERMINATING)
        return XT_SUCCESS;  // уже завершается

    process->flags |= XT_PROCESS_TERMINATING;
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);

    // Завершаем все ещё живые потоки процесса (кроме текущего)
    XTList* threadIter = process->threads;
    while (threadIter) {
        XTThread* thread = NULL;
        xtGetListData(threadIter, &thread);
        XTList* next = NULL;
        xtGetNextList(threadIter, &next);
        if (thread && thread != currentThread &&
            (thread->state & 0x7f) != XT_THREAD_TERMINATED_STATE) {
            xtTerminateThread(thread, code);
        }
        threadIter = next;
    }

    // Если текущий поток принадлежит процессу – завершаем его (переключится)
    if (currentThread && currentThread->process == process) {
        xtTerminateThread(currentThread, code);
        // сюда не вернёмся
    }

    // Если мы здесь – текущий поток из другого процесса, все потоки процесса TERMINATED

    // Освобождаем структуры потоков (они всё ещё в process->threads)
    XTList* thrIter = process->threads;
    while (thrIter) {
        XTThread* thread = NULL;
        xtGetListData(thrIter, &thread);
        XTList* next = NULL;
        xtGetNextList(thrIter, &next);
        if (thread) {
            // Освобождаем kernelStack (если есть)
            if (thread->kernelStack) {
                void* base = (uint8_t*)thread->kernelStack - 0x4000;
                xtFreePages(base, 0x4000);
            }
            // TODO: освободить context, если необходимо
            xtHeapFree(thread);
        }
        xtDestroyList(thrIter);
        thrIter = next;
    }
    process->threads = NULL;

    // Освобождаем приватные физические страницы из memoryMap
    XTList* memIter = process->memoryMap;
    while (memIter) {
        XTVirtualMap* map = NULL;
        xtGetListData(memIter, &map);
        XTList* next = NULL;
        xtGetNextList(memIter, &next);
        if (map) {
            if (map->physicalAddress && (~(map->attr & XT_MEM_SHARED))) {
                xtFreePages(map->physicalAddress, map->size);
            }
            xtHeapFree(map);
        }
        xtDestroyList(memIter);
        memIter = next;
    }
    process->memoryMap = NULL;

    // Освобождаем дерево дескрипторов
    if (process->descriptorRoot) {
        xtFreeDescriptorTree(process->descriptorRoot, HANDLE_LEVELS - 1);
        process->descriptorRoot = NULL;
    }

    for (XTList* l = processess; l; xtGetNextList(l, &l)) {
        XTProcess* ourProcess = NULL;
        xtGetListData(l, &ourProcess);
        if (ourProcess == process) {
            xtRemoveFromList(processess, l);
        }
    }

    // Освобождаем таблицу страниц
    if (process->pageTable) {
        xtFreePages(process->pageTable, 0x1000);
    }
    if (process->id < lastProcessId) {
        lastProcessId = process->id;
    }
    // Освобождаем сам процесс
    xtHeapFree(process);

    return XT_SUCCESS;
}

extern void xtUserExit();


XTResult xtCreateProcess(    
    XTProcess* parentProcess,
    uint64_t flags,
    XTProcess** out
) {

    XT_CHECK_ARG_IS_NULL(out);

    XTProcess* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTProcess), &result));

    void* newPageTable = NULL;
    XT_TRY(xtAllocatePages(NULL, 0x1000, &newPageTable));
    xtSetMem(newPageTable, 0, 0x1000);
    xtCopyMem((char*)newPageTable + 0x7f8, (char*)kernelPageTable + 0x7f8, 0x808);
    
    result->pageTable = newPageTable;
    result->parentProcess = parentProcess;
    result->id = lastProcessId++;
    XTList* newProcessList = NULL;
    xtCreateList(result, &newProcessList);
    if (processess == NULL) {
        processess = newProcessList;
    }
    else {
        xtAppendList(processess, newProcessList);
    }
    *out = result;
    
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




XTResult XTEXPORT xtCreateThread(
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
        xtSetCurrentThread(result);
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
    result->ticks = 1;
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
    return XT_SUCCESS;
}