
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

extern void xtSwitchTo();

XTResult xtSchedulerInit();


XTResult xtFileSystemInit();

XTResult xtInsertKernelModule();

void xtKernelMain(KernelBootInfo* bootInfo) {


    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    xtArchInit();

    XT_ASSERT(xtSchedulerInit());
    
    XT_ASSERT(xtRamDiskInit());

    XT_ASSERT(xtACPIInit());

    uint32_t* framebuffer = bootInfo->framebuffer;

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

    xtInsertKernelModule();
    xtDebugPrint("Load user program\n");
    XT_ASSERT(
        xtExecuteProgram(
            process,
            args,
            NULL
        );
    );
    xtDebugPrint("Load kernel module\n");
    void* base = NULL;
    uint64_t flags = 0;
    XT_ASSERT(
        xtLoadKernelModule("/initrd/ahci.xtd", &base)
    );

    xtSwitchTo();

    while(1);
    return;

}
