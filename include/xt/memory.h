#ifndef __XT_MEMORY_H__
#define __XT_MEMORY_H__

#include <stddef.h>
#include <stdint.h>

#include <xt/result.h>

typedef struct XTHeap XTHeap;

#define MEM_FREE     0
#define MEM_RESERVED 1
#define MEM_ACPI     2
#define MEM_NVS      3
#define MEM_UNUSABLE 4

XTResult xtHeapAlloc(uint64_t size, void** out);
XTResult xtHeapFree(void* ptr);

#endif