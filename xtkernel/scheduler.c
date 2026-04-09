#include <xt/scheduler.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/queue.h>
#include <xt/list.h>
#include <xt/random.h>

XTList* threads = NULL;
XTList* currentThreadIterator = NULL;

uint64_t threadCount = 0;


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


XTResult xtFindThread(XTThread* thread, XTList* l, XTList** out) {
    XT_CHECK_ARG_IS_NULL(thread);
    XT_CHECK_ARG_IS_NULL(out);

    for (XTList* i = l; i; xtGetNextList(i, &i)) {
        XTThread* threadi = NULL;
        xtGetListData(i, &threadi);
        if (threadi == thread) {
            *out = i;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;

}


XTResult xtSetResult(
    XTThread* thread,
    uint64_t result
);

XTResult
xtWaitForMultipleObjects(
    XTThread* thread,
    XTWaitable** waits,
    uint64_t countOfWaits,
    uint64_t* index
) {

    thread->waits = waits;
    thread->countOfWaits = countOfWaits;
    for (uint64_t i = 0; i < countOfWaits; ++i) {
        XTList* newThreadList = NULL;
        XT_TRY(xtCreateList(thread, &newThreadList));
        if (waits[i]->threads == NULL) {
            waits[i]->threads = newThreadList;
        }
        else {
            XT_TRY(xtAppendList(waits[i]->threads, newThreadList));
        }
    }
    thread->state = (thread->state & 0x80) | XT_THREAD_WAIT_STATE;
    XTResult result = xtSwitchToThread();
    *index = result;
    // for (uint64_t i = 0; i < countOfWaits; ++i) {
    //     XTList* threadList = NULL;
    //     xtFindThread(thread, waits[i]->threads, &threadList);
    //     if (waits[i]->threads == threadList) {
    //         xtDestroyList(threadList);
    //         waits[i]->threads = NULL;
    //     }
    //     else {
    //         xtRemoveFromList(waits[i]->threads, threadList);
    //     }
    // }
    thread->waits = NULL;
    thread->countOfWaits = 0;
    return XT_SUCCESS;
}

XTResult
xtWakeUp(
    XTWaitable* waitable
) {
    for (XTList* i = waitable->threads; i; ) {
        XTThread* thread = NULL;
        xtGetListData(i, &thread);
        for (uint64_t j = 0; j < thread->countOfWaits; ++j) {
            if (thread->waits[j] == waitable) {
                xtSetResult(thread, j);
            }
        }
        thread->state = (thread->state & 0x80) | XT_THREAD_RUN_STATE;
        XTList* prev = i;
        xtGetNextList(i, &i);
        // xtRemoveFromList(waitable->threads, prev);
    }
    // xtDestroyList(waitable->threads);
    waitable->threads = NULL;
    return XT_SUCCESS;
}

XTResult XTEXPORT xtGetTime(uint64_t* unixtime) {

}

XTResult XTEXPORT xtSetTime(uint64_t unixtime) {
    
}

XTResult xtWakeUpThreads() {
    for (XTList* i = threads; i; xtGetNextList(i, &i)) {
        XTThread* thread = NULL;
        xtGetListData(i, &thread);
        if (thread->state == XT_THREAD_SLEEP_STATE) {
            thread->ticks -= 10;
            if (thread->ticks == 0) {
                thread->state = (thread->state & 0x80) | XT_THREAD_RUN_STATE;
                thread->ticks = thread->privilage;
            }
        }
    }
    return XT_SUCCESS;
}

void xtRegDump();

XTThread* IdleThread = NULL;

uint64_t ticks = 0;

void xtSchedule() {
    // 1. Сначала будим все потоки, которые пора разбудить
    ++ticks;
    xtWakeUpThreads();
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    // 2. Уменьшаем квант времени текущего потока
    if ((currentThread->state & 0xf) == XT_THREAD_RUN_STATE) {
        currentThread->ticks -= 1;
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
    currentThread->ticks = currentThread->privilage;
    xtSetCurrentThread(currentThread);
}

void xtHalt();

void xtIdleThreadFunction() {
    while(1) xtHalt();
}

extern void* kernelPageTable;

XTProcess kernelProcess = {
    0
};


XTResult xtSchedulerInit() {
    kernelProcess.pageTable = kernelPageTable;

    XT_TRY(xtCreateThread(&kernelProcess, xtIdleThreadFunction, 0, NULL, XT_THREAD_RUN_STATE, &IdleThread));
    return XT_SUCCESS;
}
