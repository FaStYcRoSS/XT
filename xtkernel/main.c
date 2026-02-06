
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>

XTResult xtMemoryInit();

XTResult xtArchInit();

KernelBootInfo* gKernelBootInfo = NULL;

void xtKernelMain(KernelBootInfo* bootInfo) {

    gKernelBootInfo = bootInfo;
    xtArchInit();
    xtDebugPrint("hello, world from kernel!\n");
    xtMemoryInit();

    xtDebugPrint("memory was inited\n");

    void* out = 0;

    XT_ASSERT(xtHeapAlloc(0x10, &out));
    xtDebugPrint("allocate %p\n", out);
    XT_ASSERT(xtHeapFree(out));

    xtDebugPrint("free %p\n", out);

    while(1);
    return;

}
