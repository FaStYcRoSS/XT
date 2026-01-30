#ifndef __XT_MEMORY_H__
#define __XT_MEMORY_H__

#include <stddef.h>
#include <stdint.h>

#include <xt/result.h>

typedef struct XTHeap XTHeap;

XTResult xtHeapAlloc(uint64_t size, void** out);
XTResult xtHeapFree(void* ptr);

#endif