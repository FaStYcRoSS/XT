#include <xt/event.h>
#include <xt/memory.h>
#include <xt/scheduler.h>

XTResult xtCreateEvent(XTEvent** event) {
    XTEvent* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTEvent), &result));
    result->isSignaled = 0;
    xtInitSpinlock(&result->lock);
    result->list = NULL;
    *event = result;
    return XT_SUCCESS;
}

XTResult xtResetEvent(XTEvent* event) {
    XT_CHECK_ARG_IS_NULL(event);
    xtLockSpinlock(&event->lock);
    event->isSignaled = 0;
    xtUnlockSpinlock(&event->lock);
    return XT_SUCCESS;
}

XTResult xtSignalEvent(XTEvent* event) {
    XT_CHECK_ARG_IS_NULL(event);
    xtLockSpinlock(&event->lock);
    event->isSignaled = 1;
    // Будим всех ждущих — они сами разберутся кто победил
    while (event->list) {
        XTThread* thread = NULL;
        xtGetListData(event->list, &thread);
        xtWakeUpThread(thread);
        XTList* next = NULL;
        xtGetNextList(event->list, &next);
        xtDestroyList(event->list);
        event->list = next;
    }
    xtUnlockSpinlock(&event->lock);
    return XT_SUCCESS;
}

// Удалить себя из списков всех событий (после пробуждения)
static void removeFromAllEvents(XTEvent** events, uint64_t count, XTThread* thread) {
    for (uint64_t i = 0; i < count; ++i) {
        xtLockSpinlock(&events[i]->lock);
        XTList* prev = NULL;
        XTList* cur = events[i]->list;
        while (cur) {
            XTThread* t = NULL;
            xtGetListData(cur, &t);
            XTList* next = NULL;
            xtGetNextList(cur, &next);
            if (t == thread) {
                if (prev == NULL) events[i]->list = next;
                else xtSetNextList(prev, next); // нужна такая функция в list API
                xtDestroyList(cur);
                break;
            }
            prev = cur;
            cur = next;
        }
        xtUnlockSpinlock(&events[i]->lock);
    }
}

XTResult xtWaitForEvents(
    XTEvent** events,
    uint64_t  count,
    uint64_t  flags,
    uint64_t  milliseconds,
    uint64_t* index
) {
    XT_CHECK_ARG_IS_NULL(events);
    if (count == 0) return XT_SUCCESS;

    XTThread* current = NULL;
    xtGetCurrentThread(&current);

    while (1) {
        // Фаза 1: берём все локи сразу и проверяем состояние
        for (uint64_t i = 0; i < count; ++i)
            xtLockSpinlock(&events[i]->lock);

        if (flags & XT_WAIT_FOR_ALL) {
            // Проверяем что ВСЕ сигнализированы
            int allSignaled = 1;
            for (uint64_t i = 0; i < count; ++i) {
                if (!events[i]->isSignaled) { allSignaled = 0; break; }
            }
            if (allSignaled) {
                for (uint64_t i = 0; i < count; ++i) {
                    events[i]->isSignaled = 0;
                    xtUnlockSpinlock(&events[i]->lock);
                }
                return XT_SUCCESS;
            }
        } else {
            // Достаточно одного
            for (uint64_t i = 0; i < count; ++i) {
                if (events[i]->isSignaled) {
                    if (index) *index = i;
                    events[i]->isSignaled = 0;
                    for (uint64_t j = 0; j < count; ++j)
                        xtUnlockSpinlock(&events[j]->lock);
                    return XT_SUCCESS;
                }
            }
        }

        // Фаза 2: регистрируемся во всех событиях пока держим локи
        // (нет окна для потери сигнала)
        for (uint64_t i = 0; i < count; ++i) {
            XTList* node = NULL;
            xtCreateList(current, &node);
            if (!events[i]->list) events[i]->list = node;
            else xtAppendList(events[i]->list, node);
        }

        // Атомарно переходим в WAIT и отпускаем все локи
        for (uint64_t i = 0; i < count; ++i)
            xtUnlockSpinlock(&events[i]->lock);

        // Засыпаем
        if (milliseconds != UINT64_MAX) {
            xtSleepThread(current, milliseconds);
        } else {
            xtSwitchToThread();
        }

        // Проснулись — чистим себя из всех очередей
        removeFromAllEvents(events, count, current);

        // Проверяем таймаут
        if (milliseconds != UINT64_MAX && current->ticks == 0) {
            return XT_TIMEOUT;
        }
        // Иначе повторяем проверку в начале цикла
    }
}