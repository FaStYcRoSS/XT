
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/io.h>
#include <xt/acpi.h>
#include <xt/linker.h>
#include <xt/user.h>
#include <xt/queue.h>
#include <xt/string.h>

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

XTResult xtSchedulerInit();

XTResult xtFileSystemInit();

XTResult xtInsertKernelModule();

extern XTList* threads;

// void waitThread(XTProcess* process) {
//     xtDuplicateHandle(
//         process,
//         XT_STDOUT,
//         XT_FILE_MODE_READ | XT_FILE_MODE_WRITE,
//         XT_DESCRIPTOR_TYPE_FILE,
//         gSerialDevice
//     );

//     const char* args[] = {
//         "/initrd/xtinit.xte",
//         NULL
//     };

//     XTThread* mainThread = NULL;
//     xtExecuteProgram(
//         process,
//         args,
//         NULL,
//         &mainThread
//     );
//     XTThread* currentThread = NULL;
//     xtGetCurrentThread(&currentThread);
//     XTWaitable* waits[] = {
//         &mainThread->waitable
//     };
//     uint64_t index = 0;
//     XT_ASSERT(xtWaitForMultipleObjects(
//        currentThread,
//        waits,
//        1,
//        &index
//     ));
//     xtDebugPrint("thread result is %llx index is %llx\n", mainThread->result, index);
//     xtTerminateThread(currentThread, XT_SUCCESS);
//     return;
// }

extern XTProcess kernelProcess;

void Thread(void* arg) {
    while (1) {
        xtDebugPrint("thread %llx\n", arg);
        xtSchedule();
    }

}


void xtKernelMain(KernelBootInfo* bootInfo) {
    asm volatile("cli;");
    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();
    XT_ASSERT(xtACPIInit());
    xtArchInit();
    xtInsertKernelModule();

    XTThread* firstThread = NULL, *secondThread = NULL;
    xtCreateThread(
        NULL,
        Thread, 0, (void*)1, XT_THREAD_RUN_STATE, &firstThread);
    xtCreateThread(NULL,
        Thread, 0, (void*)2, XT_THREAD_RUN_STATE, &secondThread);
    
    xtStartScheduler();   // вместо Thread1();
    while(1);             // никогда не выполнится
}