
#include <xt/scheduler.h>
#include <xt/list.h>
#include <xt/spinlock.h>


typedef struct XTMutex {
    XTSpinlock lock;
    XTThread*  owner;     // кто держит
    XTList*    waitList;  // кто ждёт
} XTMutex;

XTResult xtLockMutex(XTMutex* m) {
    xtLockSpinlock(&m->lock);
    XTThread* current = NULL;
    xtGetCurrentThread(&current);
    while (m->owner != NULL) {
        // Регистрируемся и атомарно засыпаем
        XTList* node = NULL;
        xtCreateList(current, &node);
        if (!m->waitList) m->waitList = node;
        else xtAppendList(m->waitList, node);
        xtUnlockSpinlock(&m->lock);
        xtSleepThread(current, UINT64_MAX);
        xtLockSpinlock(&m->lock);
    }
    m->owner = current;
    xtUnlockSpinlock(&m->lock);
    return XT_SUCCESS;
}

XTResult xtUnlockMutex(XTMutex* m) {
    xtLockSpinlock(&m->lock);
    m->owner = NULL;
    if (m->waitList) {
        XTThread* next = NULL;
        xtGetListData(m->waitList, &next);
        XTList* rest = NULL;
        xtGetNextList(m->waitList, &rest);
        xtDestroyList(m->waitList);
        m->waitList = rest;
        xtWakeUpThread(next);
    }
    xtUnlockSpinlock(&m->lock);
    return XT_SUCCESS;
}