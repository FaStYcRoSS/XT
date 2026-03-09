#include <xt/arch/x86_64.h>
#include <xt/kernel.h>
#include <xt/memory.h>

XTResult xtDTInit();
XTResult xtEarlyRandomInit();

extern void* kernelPageTable;

XTResult xtArchInit() {
    asm volatile("mov %%cr3, %%rax":"=a"(kernelPageTable));
    XT_TRY(xtDTInit());
    XT_TRY(xtSyscallInit());
    XT_TRY(xtEarlyRandomInit());
    

    void* vdso = NULL;
    XT_ASSERT(xtFindSection(KERNEL_IMAGE_BASE, ".vdso", &vdso));
    XT_ASSERT(xtGetPhysicalAddress(kernelPageTable, vdso, &vdso));

    xtSetPages(kernelPageTable, 
        (void*)(0x00007ffffffff000),
        vdso,
        0x1000,
        XT_MEM_EXEC | XT_MEM_READ | XT_MEM_USER
    );
    return XT_SUCCESS;
}