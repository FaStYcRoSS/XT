#ifndef __XT_MEMORY_H__
#define __XT_MEMORY_H__

#include <stddef.h>
#include <stdint.h>

#include <xt/result.h>

typedef struct XTHeap XTHeap;

#define MEM_UNKNOWN  0
#define MEM_FREE     1
#define MEM_RESERVED 2
#define MEM_ACPI     3
#define MEM_NVS      4
#define MEM_UNUSABLE 5
#define MEM_MMIO     6

#define MEM_USER   0x200
#define MEM_READ   0x400
#define MEM_WRITE  0x800

XTResult xtHeapAlloc(uint64_t size, void** out);
XTResult xtHeapFree(void* ptr);

XTResult xtAllocatePages(void* address, uint64_t size, void** ptr);
XTResult xtFreePages(void* ptr, uint64_t size);

XTResult xtSetPages(void* pageTable, void* virtual_address, void* physical_address, uint64_t size, uint64_t attributes);

XTResult xtUnsetPages(void* pageTable, void* virtual_address, uint64_t size);



#endif