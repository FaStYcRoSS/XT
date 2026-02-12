#include <xt/scheduler.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/ringQueue.h>
#include <xt/list.h>

XTThread* currentThread = NULL;

XTThread threads[64] = {

};

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
    for (uint64_t i = 0; i < threadCount; ++i) {
        if (threads[i].state == XT_THREAD_SLEEP_STATE) {
            threads[i].ticks -= 10;
            if (threads[i].ticks == 0) {
                xtWakeUpThread(&threads[i]);
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
    XTThread* startThread = currentThread;
    
    while (1) {
        // Переходим к следующему
        currentThread++;
        if (currentThread >= threads + threadCount) {
            currentThread = threads;
        }

        // Если нашли поток, который готов бежать — выходим из цикла поиска
        if (currentThread->state == XT_THREAD_RUN_STATE) {
            break;
        }

        // Защита от бесконечного цикла: если обошли все и никто не готов
        if (currentThread == startThread) {
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

XTResult xtCreateThread(PFNXTTHREADFUNC ThreadFunc, size_t size, void* arg, uint8_t state, XTThread** out) {
    
    XT_CHECK_ARG_IS_NULL(ThreadFunc);
    XT_CHECK_ARG_IS_NULL(out);

    uint64_t* stack_raw;
    xtAllocatePages(NULL, 0x1000, (void**)&stack_raw);
    
    // 1. Находим конец страницы (стек растет вниз)
    // Обязательно используем uintptr_t для корректной арифметики!
    uintptr_t stack_top = (uintptr_t)stack_raw + 0x1000;

    // 2. Выравнивание по 16 байт и резерв 32 байта Shadow Space 
    // (по спецификации MS ABI для вызываемой функции)
    stack_top -= 40; 

    XTContext* ctx = NULL;
    xtHeapAlloc(sizeof(XTContext), &ctx);

    // Инициализируем "сохраненное" состояние
    ctx->rip = ThreadFunc;                    // Прерывания разрешены (IF=1)
    ctx->rflags = 0x202;
    ctx->rcx = arg;
    ctx->cs = 0x08;
    ctx->ss = 0x10;
    ctx->rsp = stack_top;

    XTThread* result = NULL;

    result = &threads[threadCount++];
    result->id = threadCount-1;
    if (threadCount == 1) {
        currentThread = result;
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