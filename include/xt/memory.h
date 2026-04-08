#ifndef __XT_MEMORY_H__
#define __XT_MEMORY_H__

#include <stddef.h>
#include <stdint.h>

#include <xt/result.h>

typedef struct XTHeap XTHeap;

#define XT_MEM_UNKNOWN  0
#define XT_MEM_FREE     1
#define XT_MEM_RESERVED 2
#define XT_MEM_ACPI     3
#define XT_MEM_NVS      4
#define XT_MEM_UNUSABLE 5
#define XT_MEM_MMIO     6

#define XT_MEM_SHARED 0x0100
#define XT_MEM_USER   0x0200
#define XT_MEM_READ   0x0400
#define XT_MEM_WRITE  0x0800
#define XT_MEM_EXEC   0x1000


#define __XT_USER_PTR__

XTResult xtHeapAlloc(uint64_t size, void** out);
XTResult xtHeapFree(void* ptr);

XTResult xtAllocatePages(void* address, uint64_t size, void** ptr);
XTResult xtFreePages(void* ptr, uint64_t size);

XTResult xtSetPages(void* pageTable, void* virtual_address, void* physical_address, uint64_t size, uint64_t attributes);

XTResult xtUnsetPages(void* pageTable, void* virtual_address, uint64_t size);

XTResult xtGetPhysicalAddress(void* pageTable, void* virtualAddress, void** out);

XTResult xtMemoryDump();

XTResult xtCopyFromUser(void* kernel_dest, void* __XT_USER_PTR__ user_src, uint64_t size);
XTResult xtCopyToUser(void* __XT_USER_PTR__ user_dest, void* kernel_src, uint64_t size);


#endif