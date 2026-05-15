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
    // 1. РЎРЅР°С‡Р°Р»Р° Р±СѓРґРёРј РІСЃРµ РїРѕС‚РѕРєРё, РєРѕС‚РѕСЂС‹Рµ РїРѕСЂР° СЂР°Р·Р±СѓРґРёС‚СЊ

    xtWakeUpThreads();
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    // 2. РЈРјРµРЅСЊС€Р°РµРј РєРІР°РЅС‚ РІСЂРµРјРµРЅРё С‚РµРєСѓС‰РµРіРѕ РїРѕС‚РѕРєР°
    if ((currentThread->state & 0xf) == XT_THREAD_RUN_STATE) {
        currentThread->ticks -= 1;
        // Р•СЃР»Рё РІСЂРµРјСЏ РїРѕС‚РѕРєР° РµС‰Рµ РЅРµ РІС‹С€Р»Рѕ Рё РѕРЅ РЅРµ Р·Р°СЃРЅСѓР» СЃР°Рј, РїСЂРѕРґРѕР»Р¶Р°РµРј РІС‹РїРѕР»РЅРµРЅРёРµ
        if (currentThread->ticks > 0) return;
    }

    // 3. РС‰РµРј СЃР»РµРґСѓСЋС‰РёР№ РїРѕС‚РѕРє, РєРѕС‚РѕСЂС‹Р№ Р“РћРўРћР’ Рє РІС‹РїРѕР»РЅРµРЅРёСЋ (RUN_STATE)
    XTList* startThreadIter = currentThreadIterator;
    
    while (1) {
        // РџРµСЂРµС…РѕРґРёРј Рє СЃР»РµРґСѓСЋС‰РµРјСѓ
        
        xtGetNextList(currentThreadIterator, &currentThreadIterator);
        if (currentThreadIterator == NULL) {
            currentThreadIterator = threads;
        }
        xtGetListData(currentThreadIterator, &currentThread);
        // Р•СЃР»Рё РЅР°С€Р»Рё РїРѕС‚РѕРє, РєРѕС‚РѕСЂС‹Р№ РіРѕС‚РѕРІ Р±РµР¶Р°С‚СЊ вЂ” РІС‹С…РѕРґРёРј РёР· С†РёРєР»Р° РїРѕРёСЃРєР°
        if ((currentThread->state & 0xf) == XT_THREAD_RUN_STATE) {
            break;
        }

        // Р—Р°С‰РёС‚Р° РѕС‚ Р±РµСЃРєРѕРЅРµС‡РЅРѕРіРѕ С†РёРєР»Р°: РµСЃР»Рё РѕР±РѕС€Р»Рё РІСЃРµ Рё РЅРёРєС‚Рѕ РЅРµ РіРѕС‚РѕРІ
        if (currentThreadIterator == startThreadIter) {
            // Р’ РёРґРµР°Р»Рµ С‚СѓС‚ РЅСѓР¶РЅРѕ РїРµСЂРµС…РѕРґРёС‚СЊ РІ Idle-РїРѕС‚РѕРє (РѕР¶РёРґР°РЅРёРµ РїСЂРµСЂС‹РІР°РЅРёСЏ)
            // РќРѕ РґР»СЏ РЅР°С‡Р°Р»Р° РїСЂРѕСЃС‚Рѕ РІС‹Р№РґРµРј
            currentThread = IdleThread;
            break;
        }
    }

    // РЎР±СЂР°СЃС‹РІР°РµРј РєРІР°РЅС‚ РІСЂРµРјРµРЅРё РґР»СЏ РІС‹Р±СЂР°РЅРЅРѕРіРѕ РїРѕС‚РѕРєР°
    currentThread->ticks = currentThread->privilage;
    xtSetCurrentThread(currentThread);
}

void xtHalt();

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
}
