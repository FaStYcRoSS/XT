#include <xt/scheduler.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/queue.h>
#include <xt/list.h>
#include <xt/random.h>

XTThread* currentThread = NULL;

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

XTProcess kernelProcess = {
    0
};


XTResult xtSchedulerInit() {
    kernelProcess.pageTable = kernelPageTable;

    XT_TRY(xtCreateThread(&kernelProcess, xtIdleThreadFunction, 0, NULL, XT_THREAD_RUN_STATE, &IdleThread));
    return XT_SUCCESS;
}

