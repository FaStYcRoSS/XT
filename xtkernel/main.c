
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>

#if defined(__x86_64__)
#include <xt/arch/x86_64.h>
#endif

XTResult xtMemoryInit();
XTResult xtSerialInit();
XTResult xtArchInit();

XTResult xtFindSection(void* image, const char* sectionName, void** out);

KernelBootInfo* gKernelBootInfo = NULL;

#define KERNEL_IMAGE_BASE ((void*)(0xffffffff80000000))

void Func1(const char* arg) {
    while (1) {
        xtDebugPrint("%s\n", arg);
    }
}

extern XTThread* currentThread;
extern XTThread* nextThread;
extern void xtStartScheduler();

void xtKernelMain(KernelBootInfo* bootInfo) {


    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    xtArchInit();

    xtCreateThread(Func1, 0x1000, "Thread 1", &currentThread);
    xtCreateThread(Func1, 0x1000, "Thread 2", &nextThread);

    xtStartScheduler();

    while(1);
    return;

}
