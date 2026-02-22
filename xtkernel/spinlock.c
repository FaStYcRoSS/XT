#include <xt/spinlock.h>
#include <xt/memory.h>
#include <stdatomic.h>

atomic_flag example_lock_variable = ATOMIC_FLAG_INIT;

void acquire( atomic_flag * lock )
{
    while( atomic_flag_test_and_set_explicit( lock, memory_order_acquire ) )
    {
        /* use whatever is appropriate for your target arch here */
        __builtin_ia32_pause();
    }
}

void release( atomic_flag * lock )
{
    atomic_flag_clear_explicit( lock, memory_order_release );
}

struct XTSpinlock {
    atomic_flag isBusy;
    void* data;
};

XTResult xtCreateSpinlock(void* data, XTSpinlock** out) {
    XT_CHECK_ARG_IS_NULL(out);
    XTSpinlock* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTSpinlock), &result));
    result->data = data;
    result->isBusy.__val = 0;
    *out = result;
    return XT_SUCCESS;

}

XTResult xtDestroySpinlock(XTSpinlock* lock) {
    return xtHeapFree(lock);
}

XTResult xtSpinlockGet(XTSpinlock* lock, void** out) {
    XT_CHECK_ARG_IS_NULL(out);
    XT_CHECK_ARG_IS_NULL(lock);
    acquire(&lock->isBusy);
    *out = lock->data;
    release(&lock->isBusy);
    return XT_SUCCESS;
}

XTResult xtSpinlockSet(XTSpinlock* lock, void* data) {
    XT_CHECK_ARG_IS_NULL(lock);
    acquire(&lock->isBusy);
    lock->data = data;
    release(&lock->isBusy);
    return XT_SUCCESS;
}