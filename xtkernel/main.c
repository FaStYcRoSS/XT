
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/io.h>
#include <xt/acpi.h>
#include <xt/linker.h>
#include <xt/user.h>

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

    XT_ASSERT(xtACPIInit());

    xtArchInit();

    XT_ASSERT(xtSchedulerInit());
    
    XT_ASSERT(xtRamDiskInit());



    uint32_t* framebuffer = bootInfo->framebuffer;

    XTProcess* process = NULL;
    XT_ASSERT(xtCreateProcess(
        NULL,
        0,
        &process
    ));

    xtDuplicateHandle(
        process,
        XT_STDOUT,
        XT_FILE_MODE_READ | XT_FILE_MODE_SHARE | XT_FILE_MODE_WRITE,
        XT_DESCRIPTOR_TYPE_FILE,
        gSerialDevice
    );

    const char* args[] = {
        "/initrd/xtinit.xte",
        NULL
    };
    xtDebugPrint("Hello from kernel!");

    xtInsertKernelModule();
    XT_ASSERT(
        xtExecuteProgram(
            process,
            args,
            NULL
        );
    );
    void* base = NULL;
    uint64_t flags = 0;
    XT_ASSERT(
        xtLoadKernelModule("/initrd/ahci.xtd", &base)
    );
    xtSwitchTo();

    while(1);
    return;

}
