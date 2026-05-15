#include <xt/scheduler.h>
<<<<<<< HEAD
void xtSwitchTo(XTThread* prev, XTThread* next);

void xtSchedule() {
    XTThread* current = NULL;
    xtGetCurrentThread(&current);
    XTThread* gFirstThread = NULL;
    asm volatile("mov %%gs:8, %%rax":"=a"(gFirstThread));
    if (current == NULL)
        xtKernelPanic("Current is NULL", NULL, XT_ACCESS_VIOLATION);
    if (current->flags == 1) {
        --current->ticks;
        if (current->ticks > 0) return;
=======
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
    xtLockSpinlock(&thread->lock);
    XT_CHECK_ARG_IS_NULL(thread);

    thread->state = XT_THREAD_SLEEP_STATE;
    thread->ticks = milliseconds;
    xtUnlockSpinlock(&thread->lock);
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    if (currentThread == thread)
        xtSwitchToThread();
    return XT_SUCCESS;
}

XTResult xtWakeUpThread(XTThread* thread) {
    xtLockSpinlock(&thread->lock);
    thread->state = (thread->state & 0x7f) | XT_THREAD_RUN_STATE;
    thread->ticks = thread->privilage;
    xtUnlockSpinlock(&thread->lock);
    return XT_SUCCESS;

}

XTResult xtWakeUpThreads() {
    for (XTList* i = threads; i; xtGetNextList(i, &i)) {
        XTThread* thread = NULL;
        xtGetListData(i, &thread);
        if (thread->state == XT_THREAD_SLEEP_STATE) {
            thread->ticks -= 1;
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
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    // 2. Уменьшаем квант времени текущего потока
    if ((currentThread->state & 0xf) == XT_THREAD_RUN_STATE) {
        currentThread->ticks -= 1;
        // Если время потока еще не вышло и он не заснул сам, продолжаем выполнение
        if (currentThread->ticks > 0) return;
>>>>>>> feature/test
    }
    current->ticks = 1;
    XTThread* prev = current;
    // переключаемся на следующий, если есть, иначе на первый
    if (current->nextInQueue != NULL)
        current = current->nextInQueue;
    else
        current = gFirstThread;
    xtSwitchTo(prev, current);
}


<<<<<<< HEAD
void xtStartScheduler() {
    XTThread* gFirstThread = NULL;
    asm volatile("mov %%gs:8, %%rax\n":"=a"(gFirstThread));
    if (gFirstThread == NULL)
        xtKernelPanic("No threads to schedule", NULL, XT_ACCESS_VIOLATION);
    xtSetCurrentThread(gFirstThread);
    XTThread* current = gFirstThread;
    XTThread dummy = {0};   // фиктивный предыдущий поток
    xtSwitchTo(&dummy, current);
    // сюда никогда не вернёмся
=======
void xtIdleThreadFunction() {
    while(1) {
        xtHalt();
    }
}

extern void* kernelPageTable;

XTProcess kernelProcess = {
    0
};


XTResult xtSchedulerInit() {
    kernelProcess.pageTable = kernelPageTable;

    XT_TRY(xtCreateThread(&kernelProcess, xtIdleThreadFunction, 0, NULL, XT_THREAD_RUN_STATE, &IdleThread));
    return XT_SUCCESS;
>>>>>>> feature/test
}
