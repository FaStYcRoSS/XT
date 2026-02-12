#include <xt/pe.h>
#include <xt/memory.h>
#include <xt/arch/x86_64.h>
#include <xt/io.h>
#include <xt/scheduler.h>

extern void* kernelPageTable;

XTResult xtCopyMem(void* dst, void* src, uint64_t size);
XTResult xtSetMem(void* dst, uint8_t c, uint64_t size);

XTResult xtCreateProcess(XTProcess** out) {

    void* newPageTable = NULL;
    xtAllocatePages(NULL, 0x1000, &newPageTable);
    xtSetMem(newPageTable, 0, 0x1000);
    xtCopyMem(newPageTable, kernelPageTable, 0x1000);
    

}