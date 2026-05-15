#include <xt/rwlock.h>
#include <xt/kernel.h>
#include <xt/scheduler.h>

XTResult xtInitRWLock(XTRWLock* lock) {
    xtInitSpinlock(&lock->spinlock);
    lock->readers     = 0;
    lock->waitReaders = NULL;
    lock->waitWriters = NULL;
    return XT_SUCCESS;
}

// --- Вспомогательная функция: усыпить текущий поток в очередь ---
static XTResult rwSleep(XTList** queue) {
    XTThread* current = NULL;
    xtGetCurrentThread(&current);
    XTList* node = NULL;
    XT_TRY(xtCreateList(current, &node));
    if (*queue == NULL) *queue = node;
    else xtAppendList(*queue, node);

    // Помечаем себя спящим и уходим — спинлок будет отпущен вызывающим
    current->state = (current->state & 0x80) | XT_THREAD_WAIT_STATE;
    return XT_SUCCESS;
}

// --- Вспомогательная функция: разбудить всю очередь ---
static void rwWakeAll(XTList** queue) {
    for (XTList* i = *queue; i; xtGetNextList(i, &i)) {
        XTThread* t = NULL;
        xtGetListData(i, &t);
        t->state = (t->state & 0x80) | XT_THREAD_RUN_STATE;
    }
    *queue = NULL;
}

// --- Вспомогательная функция: разбудить один поток ---
static void rwWakeOne(XTList** queue) {
    if (!*queue) return;
    XTThread* t = NULL;
    xtGetListData(*queue, &t);
    t->state = (t->state & 0x80) | XT_THREAD_RUN_STATE;
    XTList* next = NULL;
    xtGetNextList(*queue, &next);
    xtDestroyList(*queue);
    *queue = next;
}

XTResult xtAcquireRead(XTRWLock* lock) {
    xtLockSpinlock(&lock->spinlock);
    while (lock->readers == -1 || lock->waitWriters != NULL) {
        // Есть писатель или писатель ждёт — пишущие в приоритете,
        // чтобы избежать writer starvation
        XT_TRY(rwSleep(&lock->waitReaders));
        xtUnlockSpinlock(&lock->spinlock);
        xtSwitchToThread();
        xtLockSpinlock(&lock->spinlock);
    }
    lock->readers++;
    xtUnlockSpinlock(&lock->spinlock);
    return XT_SUCCESS;
}

XTResult xtReleaseRead(XTRWLock* lock) {
    xtLockSpinlock(&lock->spinlock);
    lock->readers--;
    if (lock->readers == 0) {
        // Последний читатель — будим одного писателя (приоритет)
        rwWakeOne(&lock->waitWriters);
    }
    xtUnlockSpinlock(&lock->spinlock);
    return XT_SUCCESS;
}

XTResult xtAcquireWrite(XTRWLock* lock) {
    xtLockSpinlock(&lock->spinlock);
    while (lock->readers != 0) {
        // Есть читатели или другой писатель
        XT_TRY(rwSleep(&lock->waitWriters));
        xtUnlockSpinlock(&lock->spinlock);
        xtSwitchToThread();
        xtLockSpinlock(&lock->spinlock);
    }
    lock->readers = -1;  // сигнал: "я — писатель"
    xtUnlockSpinlock(&lock->spinlock);
    return XT_SUCCESS;
}

XTResult xtReleaseWrite(XTRWLock* lock) {
    xtLockSpinlock(&lock->spinlock);
    lock->readers = 0;
    if (lock->waitWriters != NULL) {
        // Сначала писатели — избегаем starvation
        rwWakeOne(&lock->waitWriters);
    } else {
        // Нет писателей — будим всех читателей
        rwWakeAll(&lock->waitReaders);
    }
    xtUnlockSpinlock(&lock->spinlock);
    return XT_SUCCESS;
}