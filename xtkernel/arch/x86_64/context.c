#include <xt/arch/x86_64.h>
#include <xt/scheduler.h>


extern void* kernelPageTable;

extern void xtUserExit();

XTResult xtSetContext(
    XTProcess* process, 
    void* kernelStack,
    uint64_t function, 
    uint64_t arg,
    uint64_t physStackTop,
    uint64_t stackTop, 
    uint64_t flags,
    void** context
) {

    uint64_t* ret = (uint64_t*)physStackTop;
    uint64_t userExitPtr = 0;
    xtGetPhysicalAddress(kernelPageTable, xtUserExit, &userExitPtr);
    userExitPtr = (USER_SIZE_MAX) + ((uint64_t)userExitPtr & 0xfff);
    *ret = userExitPtr;

    XTContext* ctx = (XTContext*)((char*)kernelStack + 0x4000 - sizeof(XTContext));

    // Инициализируем "сохраненное" состояние
    ctx->rip = function;                    // Прерывания разрешены (IF=1)
    ctx->rflags = 0x202;
    ctx->rcx = arg;
    if (flags & XT_THREAD_USER) {
        ctx->cs = 0x1b;
        ctx->ss = 0x23;
    }
    else {
        ctx->cs = 0x08;
        ctx->ss = 0x10;
    }
    ctx->rsp = stackTop;
    *context = ctx;
    return XT_SUCCESS;
}