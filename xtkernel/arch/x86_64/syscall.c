#include <xt/result.h>
#include <stdint.h>
#include <xt/arch/x86_64.h>
#include <xt/user.h>

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


XTResult xtSyscallInit() {
    uint64_t efer = 0;
    xtRdmsr(0xc0000080, &efer);
    xtWrmsr(0xc0000080, efer | 0x1 | EFER_NXE);
    xtWrmsr(0xc0000081, ((0x10ull << 16 )| 0x08ull) << 32);
    xtWrmsr(0xc0000082, xtSyscallHandler);
    return XT_SUCCESS;
}