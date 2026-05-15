#include <xt/spinlock.h>
#include <xt/memory.h>
#include <stdatomic.h>

atomic_flag example_lock_variable = ATOMIC_FLAG_INIT;

void xtPause();
void xtClearInterrupts();
void xtSetInterrupts();

void acquire( atomic_flag * lock )
{
    while( atomic_flag_test_and_set_explicit( lock, memory_order_acquire ) )
    {
        /* use whatever is appropriate for your target arch here */
        xtPause();
    }
}

void release( atomic_flag * lock )
{
    atomic_flag_clear_explicit( lock, memory_order_release );
}

XTResult xtInitSpinlock(XTSpinlock* lock) {
    lock->isBusy.__val = 0;
    return XT_SUCCESS;
}

XTResult xtLockSpinlock(XTSpinlock* lock) {
    xtClearInterrupts();
    acquire(&lock->isBusy);
    return XT_SUCCESS;
}

XTResult xtUnlockSpinlock(XTSpinlock* lock) {
    release(&lock->isBusy);
    xtSetInterrupts();
    return XT_SUCCESS;
}
