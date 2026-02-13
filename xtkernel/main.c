
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/io.h>

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

XTThread* firstThread = NULL;
XTThread* secondThread = NULL;

void Func1() {
    for (uint64_t i = 0; i < 5; ++i) {
        xtDebugPrint("Thread 0x%x\n", currentThread->id);
        if (currentThread == firstThread)
            xtSleepThread(currentThread, 1000);
    }
    xtTerminateThread(currentThread, XT_SUCCESS);
}


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

    xtDebugPrint("boot initrd 0x%llx\nframebuffer 0x%llx width=%u height %u\n", 
        bootInfo->initrd, bootInfo->framebuffer, bootInfo->width, bootInfo->height);
    
    XT_ASSERT(xtRamDiskInit());

    uint32_t* framebuffer = bootInfo->framebuffer;

    void* vdso = NULL;
    xtFindSection(KERNEL_IMAGE_BASE, ".vdso", &vdso);
    xtGetPhysicalAddress(kernelPageTable, vdso, &vdso);

    xtSetPages(kernelPageTable, 
        (void*)(0x00007ffffffff000),
        vdso,
        0x1000,
        XT_MEM_EXEC | XT_MEM_READ | XT_MEM_WRITE | XT_MEM_USER
    );

    XTFile* xtinit = NULL;
    uint64_t size = 0;
    void* initbase = 0;
    XT_ASSERT(xtOpenFile("/initrd/xtinit.xte", XT_FILE_MODE_READ, &xtinit));
    XT_ASSERT(xtMapFile(xtinit, &size, &initbase));
    xtDebugPrint("initbase 0x%llx size 0x%llx\n", initbase, size);
    userProgram = initbase;

    XT_ASSERT(xtCreateProcess(
        NULL,
        0,
        NULL
    ));

    xtSwitchTo();

    while(1);
    return;

}
