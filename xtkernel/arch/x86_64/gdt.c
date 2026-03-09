#include <xt/arch/x86_64.h>

#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/kernel.h>
#include "../../../external/printf.h"

// Стандартный дескриптор (8 байт)
typedef struct {
    uint16_t limit0;
    uint16_t base0;
    uint8_t  base1;
    uint8_t  access;
    uint8_t  flags_limit1;
    uint8_t  base2;
} __attribute__((packed)) GDTEntry;

// Специальный дескриптор для TSS (если понадобится позже) — 16 байт
typedef struct {
    GDTEntry low;
    uint32_t base3;
    uint32_t reserved;
} __attribute__((packed)) TSSDescriptor;

typedef struct {
    uint16_t size;
    uint64_t addr;
} __attribute__((packed)) GDTR;

typedef struct {
    uint16_t size;
    uint64_t addr;
} __attribute__((packed)) IDTR;


// Индексы: 0-null, 1-kcode, 2-kdata, 3-ucode, 4-udata
GDTEntry gdt[8] = {
    {0}, // Null descriptor
    // Kernel Code (Base=0, Limit=0, Access=0x9A, Flags=0x20 [L bit set])
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0x9A, .flags_limit1 = 0x20, .base2 = 0 },
    // Kernel Data (Base=0, Limit=0, Access=0x92, Flags=0x00)
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0x92, .flags_limit1 = 0x00, .base2 = 0 },
    // User Code (Access=0xFA, Flags=0x20)
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0xFA, .flags_limit1 = 0x20, .base2 = 0 },
    // User Data (Access=0xF2, Flags=0x00)
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0xF2, .flags_limit1 = 0x00, .base2 = 0 },
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0xFA, .flags_limit1 = 0x20, .base2 = 0 },
    //tss
    { 0 },
    { 0 },
};

GDTR _gdtr = {
    .size = sizeof(gdt) - 1,
    .addr = (uint64_t)gdt
};

typedef struct TSS {
    uint32_t reserved;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t IST[7];
    uint32_t reserved2;
    uint16_t reserved3;
    uint16_t IOPB;
} __attribute__((packed)) TSS;

TSS tss = {
    0
};


typedef struct GateEntry {
    uint16_t offset;
    uint16_t segment;
    uint16_t flags;
    uint16_t offset2;
    uint64_t offset3;
} GateEntry;

GateEntry* idt = NULL;

typedef void(*PFNXTIRQFunc)(void);

IDTR idtr = {
    0
};

const char* registers[] = {
    "cr2",
    "rax", "rbx", "rcx", "rdx",
    "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11",
    "r12", "r13", "r14", "r15",
    "int", "error", "rip", "cs",
    "rflags", "rsp", "ss"
};


void xtHalt() {
    asm volatile("hlt;");
}

void xtRegDump() {
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    XTContext* context = currentThread->context;
    
    for (uint64_t i = 0; i < sizeof(registers)/sizeof(*registers);++i) {
        xtDebugPrint("% 6s=0x%016llx ", registers[i], ((uint64_t*)context)[i]);
        if (i % 4 == 0 && i != 0) xtDebugPrint("\n");
    }
    xtDebugPrint("\n");
}


void xtSendEOI();

void xtSchedule();

int returned = 0;

void xtInvalidatePage(void* page) {
    asm volatile("invlpg (%0)" :: "r" (page) : "memory");
}

void xtPageFaultHandler() {
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    XTProcess* currentProcess = NULL;
    xtGetCurrentProcess(&currentProcess);
    XTList* prev = NULL;
    XTContext* ctx = currentThread->context;
    uint64_t errorVM = ctx->cr2;
    XTVirtualMap* mapEntry = NULL;

    XTResult result = xtFindVirtualMap(
        currentProcess,
        errorVM,
        &mapEntry,
        &prev
    );
    if (result == XT_NOT_FOUND) {
        xtTerminateThread(currentThread, XT_ACCESS_DENIED);
    }
    if ((ctx->errorCode & 0x2) && !(mapEntry->attr & XT_MEM_WRITE)) {
        xtTerminateThread(currentThread, XT_ACCESS_DENIED);
    }

    void* newPage = NULL;
    XT_TRY(xtAllocatePages(NULL, 0x1000, &newPage));
    errorVM = (errorVM) & (~(0xfff));
    xtSetPages(
        currentProcess->pageTable,
        errorVM,
        newPage,
        0x1000,
        mapEntry->attr
    );
    xtInvalidatePage(errorVM);
}

const char* exceptionShortName[] = {
    "Division Error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range Exceed",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor error",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 exception",
    "Alignment check",
    "Machine check",
    "SIMD float-point exception",
    "Virtualization exception",
    "Control protection exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor injection exception",
    "VMM communication exception",
    "Security exception",
    "Reserved"
};
//bit map of exceptions
uint32_t exceptionHasError = 0x500227d00;
void xtExceptionHandler() {
    XTThread* currentThread = NULL;
    xtGetCurrentThread(&currentThread);
    XTContext* ctx = currentThread->context;

    if (ctx->interruptNumber == 0x20) {
        asm volatile("cli");
        xtSchedule();
        xtGetCurrentThread(&currentThread);
        tss.rsp0 = currentThread->kernelStack;
        xtSendEOI();
        return;
    }
    if (ctx->interruptNumber == 0xe) {
        xtPageFaultHandler();
        return;
    }
    // Обработка других исключений, например #GP (13) или #DF (8)
    char buff[64];
    int n = snprintf_(buff, 64, "%s", exceptionShortName[ctx->interruptNumber]);
    if ((exceptionHasError >> ctx->interruptNumber) & 0x1) {
        snprintf_(buff + n, 64 - n, ":Error 0x%llx", ctx->errorCode);
    }
    xtRegDump();
    xtKernelPanic(buff, ctx->rip, XT_ARCH_EXCEPTION);
}

XTResult xtSetIRQ(uint8_t vector, PFNXTIRQFunc irqFunc, uint8_t ist) {
    idt[vector].flags = 0x8e00 | ist;
    idt[vector].segment = 0x08;
    uint64_t addrFunc = (uint64_t)irqFunc;
    idt[vector].offset = addrFunc & 0xffff;
    idt[vector].offset2 = (addrFunc >> 16) & 0xffff;
    idt[vector].offset3 = (addrFunc >> 32) & 0xffffffff;
    return XT_SUCCESS;
}

extern void isr_stub_0();
extern void isr_stub_1();
extern void isr_stub_2();
extern void isr_stub_3();
extern void isr_stub_4();
extern void isr_stub_5();
extern void isr_stub_6();
extern void isr_stub_7();
extern void isr_stub_8();
extern void isr_stub_9();
extern void isr_stub_10();
extern void isr_stub_11();
extern void isr_stub_12();
extern void isr_stub_13();
extern void isr_stub_14();
extern void isr_stub_15();
extern void isr_stub_16();
extern void isr_stub_17();
extern void isr_stub_18();
extern void isr_stub_19();
extern void isr_stub_20();
extern void isr_stub_21();
extern void isr_stub_22();
extern void isr_stub_23();
extern void isr_stub_24();
extern void isr_stub_25();
extern void isr_stub_26();
extern void isr_stub_27();
extern void isr_stub_28();
extern void isr_stub_29();
extern void isr_stub_30();
extern void isr_stub_31();
extern void isr_stub_32();
extern void isr_stub_40();

void xtSetIRQs() {
    xtSetIRQ( 0, isr_stub_0, 0);
    xtSetIRQ( 1, isr_stub_1, 0);
    xtSetIRQ( 2, isr_stub_2, 0);
    xtSetIRQ( 3, isr_stub_3, 0);
    xtSetIRQ( 4, isr_stub_4, 0);
    xtSetIRQ( 5, isr_stub_5, 0);
    xtSetIRQ( 6, isr_stub_6, 0);
    xtSetIRQ( 7, isr_stub_7, 0);
    xtSetIRQ( 8, isr_stub_8, 1);
    xtSetIRQ( 9, isr_stub_9, 0);
    xtSetIRQ(10, isr_stub_10, 0);
    xtSetIRQ(11, isr_stub_11, 0);
    xtSetIRQ(12, isr_stub_12, 0);
    xtSetIRQ(13, isr_stub_13, 1);
    xtSetIRQ(14, isr_stub_14, 0);
    xtSetIRQ(15, isr_stub_15, 0);
    xtSetIRQ(16, isr_stub_16, 0);
    xtSetIRQ(17, isr_stub_17, 0);
    xtSetIRQ(18, isr_stub_18, 0);
    xtSetIRQ(19, isr_stub_19, 0);
    xtSetIRQ(20, isr_stub_20, 0);
    xtSetIRQ(21, isr_stub_21, 0);
    xtSetIRQ(22, isr_stub_22, 0);
    xtSetIRQ(23, isr_stub_23, 0);
    xtSetIRQ(24, isr_stub_24, 0);
    xtSetIRQ(25, isr_stub_25, 0);
    xtSetIRQ(26, isr_stub_26, 0);
    xtSetIRQ(27, isr_stub_27, 0);
    xtSetIRQ(28, isr_stub_28, 0);
    xtSetIRQ(29, isr_stub_29, 0);
    xtSetIRQ(30, isr_stub_30, 0);
    xtSetIRQ(31, isr_stub_31, 0);
    xtSetIRQ(32, isr_stub_32, 0);
    xtSetIRQ(40, isr_stub_40, 0);
}



XTResult xtPITInit();

XTResult xtDTInit() {
    // Важно: LGDT просто загружает указатель. 
    // Чтобы изменения вступили в силу, нужно перезагрузить сегментные регистры.
    
    idt = (GateEntry*)(0xffff800000000000);


    idtr.addr = idt;
    idtr.size = 0x1000 - 1;
    asm volatile("lidt %0" :: "m"(idtr));

    uint64_t tssStackBottom = 0;

    xtAllocatePages(NULL, 0x4000, &tssStackBottom);
    tss.IST[0] = HIGHER_MEM(tssStackBottom + 0x4000);
    uint64_t tssPtr = &tss;
    TSSDescriptor* tssDesc = &gdt[6];
    tssDesc->low.base0 = tssPtr & 0xffff;
    tssDesc->low.base1 = (tssPtr >> 16) & 0xff;
    tssDesc->low.base2 = (tssPtr >> 24) & 0xff;
    tssDesc->low.limit0 = sizeof(TSS)-1;
    tssDesc->low.flags_limit1 = 0x40;
    tssDesc->low.access = 0x89;
    tssDesc->base3 = (tssPtr >> 32) & 0xffffffff;
    tssDesc->reserved = 0;
    uint16_t tssSegment = 0x6*8;
    asm volatile("lgdt %0" : : "m"(_gdtr));

    xtPITInit();
    xtSetIRQs();


    asm volatile("ltr %%ax"::"a"(tssSegment));



    // После lgdt нужно сделать "far return" или "far jump" для обновления CS
    // И обновить селекторы данных (ds, es, ss)
    return XT_SUCCESS;
}