#ifndef __XT_RWLOCK_H__
#define __XT_RWLOCK_H__

#include <xt/spinlock.h>
#include <xt/list.h>

typedef struct XTRWLock {
    XTSpinlock    spinlock;     // защищает поля ниже
    int32_t       readers;      // >0: кол-во читателей, -1: писатель
    XTList*       waitReaders;  // потоки, ждущие чтения
    XTList*       waitWriters;  // потоки, ждущие записи
} XTRWLock;

XTResult xtInitRWLock(XTRWLock* lock);
XTResult xtAcquireRead(XTRWLock* lock);
XTResult xtReleaseRead(XTRWLock* lock);
XTResult xtAcquireWrite(XTRWLock* lock);
XTResult xtReleaseWrite(XTRWLock* lock);

#endif