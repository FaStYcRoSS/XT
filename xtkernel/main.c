
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

extern XTThread* currentThread;

XTThread* firstThread = NULL;
XTThread* secondThread = NULL;

void Func1(const char* arg) {
    for (uint64_t i = 0; i < 5; ++i) {
        xtDebugPrint("%s\n", arg);
        if (currentThread == firstThread)
            xtSleepThread(currentThread, 1000);
    }
    xtTerminateThread(currentThread, XT_SUCCESS);
}


extern void xtSwitchTo();

XTResult xtSchedulerInit();

void xtKernelMain(KernelBootInfo* bootInfo) {


    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    xtArchInit();

    xtSchedulerInit();

    xtCreateThread(Func1, 0x1000, "Thread 1", XT_THREAD_RUN_STATE, &firstThread);
    xtCreateThread(Func1, 0x1000, "Thread 2", XT_THREAD_RUN_STATE, &secondThread);

    XTThread* three = NULL;
    xtCreateThread(Func1, 0x1000, "Thread 3", XT_THREAD_RUN_STATE, &three);

    xtSwitchTo();

    while(1);
    return;

}
