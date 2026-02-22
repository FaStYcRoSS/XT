
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/io.h>
#include <xt/acpi.h>
#include <xt/linker.h>

#if defined(__x86_64__)
#include <xt/arch/x86_64.h>
#endif

XTResult xtMemoryInit();
XTResult xtSerialInit();
XTResult xtArchInit();
XTResult xtMemoryDump();

XTResult xtFindSection(void* image, const char* sectionName, void** out);

KernelBootInfo* gKernelBootInfo = NULL;

#define KERNEL_IMAGE_BASE ((void*)(0xffffffff80000000))

extern XTThread* currentThread;

extern void xtSwitchTo();

XTResult xtSchedulerInit();

void* kernelPageTable = NULL;

XTResult xtFileSystemInit();

void xtKernelMain(KernelBootInfo* bootInfo) {
    asm volatile("mov %%cr3, %%rax":"=a"(kernelPageTable));

    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    xtArchInit();

    XT_ASSERT(xtSchedulerInit());
    
    XT_ASSERT(xtRamDiskInit());

    XT_ASSERT(xtACPIInit());

    uint32_t* framebuffer = bootInfo->framebuffer;

    void* vdso = NULL;
    XT_ASSERT(xtFindSection(KERNEL_IMAGE_BASE, ".vdso", &vdso));
    XT_ASSERT(xtGetPhysicalAddress(kernelPageTable, vdso, &vdso));

    xtSetPages(kernelPageTable, 
        (void*)(0x00007ffffffff000),
        vdso,
        0x1000,
        XT_MEM_EXEC | XT_MEM_READ | XT_MEM_USER
    );

    XTProcess* process = NULL;
    XT_ASSERT(xtCreateProcess(
        NULL,
        0,
        &process
    ));

    const char* args[] = {
        "/initrd/xtinit.xte",
        NULL
    };

    XT_ASSERT(
        xtExecuteProgram(
            process,
            args,
            NULL
        );
    );


    xtSwitchTo();

    while(1);
    return;

}
