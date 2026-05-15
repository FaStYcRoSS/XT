
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/io.h>
#include <xt/acpi.h>
#include <xt/linker.h>
#include <xt/user.h>
#include <xt/queue.h>
<<<<<<< HEAD
#include <xt/string.h>
=======
#include <xt/time.h>
>>>>>>> feature/test

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

<<<<<<< HEAD
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
=======
XTResult waitThread(void* arg) {
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

    xtDuplicateHandle(
        process,
        XT_STDIN,
        XT_FILE_MODE_READ,
        XT_DESCRIPTOR_TYPE_FILE,
        gSerialDevice
    );


    xtDuplicateHandle(
        process,
        XT_STDERR,
        XT_FILE_MODE_WRITE,
        XT_DESCRIPTOR_TYPE_FILE,
        gSerialDevice
    );

    const char* args[] = {
        "/initrd/xtinit.xte",
        NULL
    };
    xtInsertKernelModule();
    XTThread* mainThread = NULL;
    XT_ASSERT(
        xtExecuteProgram(
            process,
            args,
            NULL,
            &mainThread
        );
    );
    XTResult result = NULL;
    xtWaitForThread(mainThread, &result);
    xtDebugPrint("result is 0x%llx\n", result);
    xtShutdown(XT_SHUTDOWN_POWER_OFF);
    //xtSwitchToThread();
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    xtTerminateThread(currentThread, XT_SUCCESS);
}

extern XTProcess kernelProcess;

int bKernelWasInitialised = 0;

void xtKernelMain(KernelBootInfo* bootInfo) {

    asm volatile("cli;");
    gKernelBootInfo = bootInfo;
    xtSerialInit();
    xtMemoryInit();

    XT_ASSERT(xtACPIInit());

    XT_ASSERT(xtArchInit());

    XT_ASSERT(xtSchedulerInit());
    XT_ASSERT(xtFileSystemInit());
    XT_ASSERT(xtRamDiskInit());
    XTThread* wait = NULL;

    XT_ASSERT(xtCreateThread(
        &kernelProcess,
        waitThread,
        0,
        NULL,
        XT_THREAD_RUN_STATE,
        &wait
    ));

    XTThread* thread = NULL;
    xtGetListData(threads, &thread);
    xtSetCurrentThread(thread);
    bKernelWasInitialised = 1;
    xtSwitchTo();
>>>>>>> feature/test

    XTThread* firstThread = NULL, *secondThread = NULL;
    xtCreateThread(
        NULL,
        Thread, 0, (void*)1, XT_THREAD_RUN_STATE, &firstThread);
    xtCreateThread(NULL,
        Thread, 0, (void*)2, XT_THREAD_RUN_STATE, &secondThread);
    
    xtStartScheduler();   // вместо Thread1();
    while(1);             // никогда не выполнится
}