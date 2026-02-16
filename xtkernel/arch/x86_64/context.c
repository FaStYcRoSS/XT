#include <xt/arch/x86_64.h>
#include <xt/scheduler.h>

XTResult xtCreateContext(
    void* kernelStack, 
    uint64_t state,
    uint64_t func, 
    uint64_t arg, 
    uint64_t stack
) {
    XTContext* ctx = (XTContext*)((char*)kernelStack + 0x4000 - sizeof(XTContext) - 40);

    ctx->rip = func;                    // Прерывания разрешены (IF=1)
    ctx->rflags = 0x202;
    ctx->rcx = arg;
    if (state & XT_THREAD_USER) {
        ctx->cs = 0x1b;
        ctx->ss = 0x23;
        ctx->rsp = stack;
    }
    else {
        ctx->cs = 0x08;
        ctx->ss = 0x10;
        ctx->rsp = (char*)kernelStack + 0x4000 - 40;
    }
    return XT_SUCCESS;
}