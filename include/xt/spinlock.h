#ifndef __XT_SPINLOCK_H__
#define __XT_SPINLOCK_H__

#include <xt/result.h>
#include <stdint.h>

typedef struct XTSpinlock XTSpinlock;

XTResult xtCreateSpinlock(void* data, XTSpinlock** out);
XTResult xtDestroySpinlock(XTSpinlock* lock);
XTResult xtSpinlockGet(XTSpinlock* lock, void** out);
XTResult xtSpinlockSet(XTSpinlock* lock, void* out);

#endif