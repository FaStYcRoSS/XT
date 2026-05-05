#ifndef __XT_SPINLOCK_H__
#define __XT_SPINLOCK_H__

#include <xt/result.h>
#include <stdint.h>
#include <stdatomic.h>

typedef struct XTSpinlock {
    atomic_flag isBusy;
} XTSpinlock;

XTResult xtInitSpinlock(XTSpinlock* lock);
XTResult xtLockSpinlock(XTSpinlock* lock);
XTResult xtUnlockSpinlock(XTSpinlock* lock);

#endif