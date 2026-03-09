#include <xt/result.h>
#include <stdint.h>
#include <xt/arch/x86_64.h>
#include <xt/user.h>
#include <xt/memory.h>

#define EFER_NXE (1 << 11)

extern void xtSyscallHandler();

typedef XTResult(*PFNXTSYSCALLENTRY)();

PFNXTSYSCALLENTRY xtSyscallArray[] = {
    xtUserTerminateThread,
    xtUserWriteFile,
    xtUserReadFile
};

XTResult xtWrmsr(uint32_t code, uint64_t value) {
    asm volatile("wrmsr;"::"d"(value >> 32), "a"(value & 0xffffffff), "c"(code));
    return XT_SUCCESS;
}

XTResult xtRdmsr(uint64_t code, uint64_t* value) {
    XT_CHECK_ARG_IS_NULL(value);
    uint32_t low = 0, high = 0;
    asm volatile("rdmsr;":"=d"(high), "=a"(low): "c"(code));
    *value = low | (high << 32);
    return XT_SUCCESS;
}

#define MSR_EFER           0xc0000080
#define MSR_STAR           0xc0000081
#define MSR_LSTAR          0xc0000082
#define MSR_FS_BASE        0xc0000100
#define MSR_GS_BASE        0xc0000101
#define MSR_KERNEL_GS_BASE 0xc0000102

XTResult xtSyscallInit() {
    uint64_t efer = 0;
    xtRdmsr(MSR_EFER, &efer);
    xtWrmsr(MSR_EFER, efer | 0x1 | EFER_NXE);
    xtWrmsr(MSR_STAR, ((0x18ull << 16 )| 0x08ull) << 32);
    xtWrmsr(MSR_LSTAR, xtSyscallHandler);
    XTPerCPUData* perCPU = NULL;
    xtHeapAlloc(sizeof(XTPerCPUData), &perCPU);
    xtWrmsr(MSR_KERNEL_GS_BASE, NULL);
    xtWrmsr(MSR_FS_BASE, 0);
    xtWrmsr(MSR_GS_BASE, perCPU);
    return XT_SUCCESS;
}