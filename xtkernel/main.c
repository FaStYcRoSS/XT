
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/io.h>
#include <xt/acpi.h>
#include <xt/linker.h>
#include <xt/user.h>
#include <xt/queue.h>

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

extern XTList* threads;

void xtKernelMain(KernelBootInfo* bootInfo) {

    asm volatile("cli;");
    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    XT_ASSERT(xtACPIInit());

    xtArchInit();

    XT_ASSERT(xtSchedulerInit());
    XT_ASSERT(xtFileSystemInit());
    XT_ASSERT(xtRamDiskInit());

    XTProcess* process = NULL;
    XT_ASSERT(xtCreateProcess(
        NULL,
        0,
        &process
    ));

    xtDuplicateHandle(
        process,
        XT_STDOUT,
        XT_FILE_MODE_READ | XT_FILE_MODE_WRITE,
        XT_DESCRIPTOR_TYPE_FILE,
        gSerialDevice
    );

    const char* args[] = {
        "/initrd/xtinit.xte",
        NULL
    };
    xtInsertKernelModule();
    XT_ASSERT(
        xtExecuteProgram(
            process,
            args,
            NULL
        );
    );
    for (XTList* threadI = threads; threadI; xtGetNextList(threadI, &threadI)) {
        XTThread* thread = NULL;
        xtGetListData(threadI, &thread);
        if (threads == threadI) {
            xtSetCurrentThread(thread);
        }
    }
    xtSwitchTo();

    while(1);
    return;

}
