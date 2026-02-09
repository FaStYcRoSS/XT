
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>

#if defined(__x86_64__)
#include <xt/arch/x86_64.h>
#endif

XTResult xtMemoryInit();
XTResult xtSerialInit();
XTResult xtArchInit();

XTResult xtFindSection(void* image, const char* sectionName, void** out);

KernelBootInfo* gKernelBootInfo = NULL;

#define KERNEL_IMAGE_BASE ((void*)(0xffffffff80000000))

void xtKernelMain(KernelBootInfo* bootInfo) {

    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    xtArchInit();

    uint64_t cr3 = 0;
    asm volatile("mov %%cr3, %%rax":"=a"(cr3));
    void* virtualAddr = USER_SIZE_MAX;
    xtDebugPrint("virtualAddr 0x%llx\ncr3 0x%llx\n", virtualAddr, cr3);

    void* sectionAddr = NULL;
    XT_ASSERT(xtFindSection(KERNEL_IMAGE_BASE, ".vdso", &sectionAddr));

    xtDebugPrint(".vdso 0x%llx\n", sectionAddr);
    void* physicalAddress = NULL;
    XT_ASSERT(xtGetPhysicalAddress(cr3, sectionAddr, &physicalAddress));

    xtDebugPrint("phys .vdso 0x%llx\n", physicalAddress);
    XT_ASSERT(xtSetPages(
        cr3,
        virtualAddr,
        physicalAddress,
        0x1000,
        XT_MEM_USER | XT_MEM_READ | XT_MEM_WRITE | XT_MEM_EXEC
    ));
    XT_ASSERT(xtUnsetPages(
        cr3,
        virtualAddr, 
        0x1000
    ));



    while(1);
    return;

}
