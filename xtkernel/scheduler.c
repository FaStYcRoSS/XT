#include <xt/scheduler.h>
#include <xt/kernel.h>
#include <xt/memory.h>

XTThread* currentThread = NULL;
XTThread* nextThread = NULL;

XTResult xtTerminateThread(XTThread* thread) {
    xtDebugPrint("delete thread 0x%llx\n", thread);
    return XT_NOT_IMPLEMENTED;
}

XTResult xtMemSet(void* data, uint8_t c, uint64_t count);
// Вспомогательная обертка, чтобы передать аргумент в поток правильно
void xtThreadEntryWrapper(void (*func)(void*), void* arg) {
    // В x64 MS ABI аргументы передаются через RCX, RDX
    // Но мы можем просто вызвать функцию напрямую
    func(arg);
    // Если поток завершился, тут можно сделать бесконечный цикл или halt
    for(;;);
}

void xtSchedule() {

    XTThread* old = currentThread;
    XTThread* _new = nextThread;

    nextThread = old;
    currentThread = _new;
}

XTResult xtCreateThread(PFNXTTHREADFUNC ThreadFunc, size_t size, void* arg, XTThread** out) {
    
    XT_CHECK_ARG_IS_NULL(ThreadFunc);
    XT_CHECK_ARG_IS_NULL(out);
    
    uint64_t* stack_raw;
    xtAllocatePages(NULL, 0x1000, (void**)&stack_raw);
    
    // 1. Находим конец страницы (стек растет вниз)
    // Обязательно используем uintptr_t для корректной арифметики!
    uintptr_t stack_top = (uintptr_t)stack_raw + 0x1000;

    // 2. Выравнивание по 16 байт и резерв 32 байта Shadow Space 
    // (по спецификации MS ABI для вызываемой функции)
    stack_top -= 32; 



    XTContext* ctx = NULL;
    xtHeapAlloc(sizeof(XTContext), &ctx);

    // Инициализируем "сохраненное" состояние
    ctx->rip = ThreadFunc;                    // Прерывания разрешены (IF=1)
    ctx->rflags = 0x202;
    ctx->rcx = arg;
    ctx->cs = 0x08;
    ctx->ss = 0x10;
    ctx->rsp = stack_top;

    XTThread* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTThread), &result));
    result->context = ctx;
    result->result = 0;
    *out = result;
    // Теперь нам нужна маленькая хитрость: xtThreadEntryWrapper должен 
    // знать, что вызывать. Перепишем враппер в ASM или используем регистры.
    return XT_SUCCESS;
}