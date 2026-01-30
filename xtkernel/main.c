
#include <xt/efi.h>

#include <xt/kernel.h>
#include <xt/memory.h>

XTResult xtMemoryInit(KernelBootInfo* bootInfo);

void xtKernelMain(KernelBootInfo* bootInfo) {

    kprintf("hello, world from kernel!");

    xtMemoryInit(bootInfo);
    kprintf("memory was inited");

    void* out = 0;

    xtHeapAlloc(0x10, &out);
    kprintf("allocate %p\n", out);
    xtHeapFree(out);

    kprintf("free %p\n", out);

    while(1);
    return;

}
