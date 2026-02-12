#include <xt/arch/x86_64.h>

#include <xt/memory.h>
#include <xt/scheduler.h>
#include <xt/kernel.h>

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
GDTEntry gdt[6] = {
    {0}, // Null descriptor
    // Kernel Code (Base=0, Limit=0, Access=0x9A, Flags=0x20 [L bit set])
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0x9A, .flags_limit1 = 0x20, .base2 = 0 },
    // Kernel Data (Base=0, Limit=0, Access=0x92, Flags=0x00)
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0x92, .flags_limit1 = 0x00, .base2 = 0 },
    // User Code (Access=0xFA, Flags=0x20)
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0xFA, .flags_limit1 = 0x20, .base2 = 0 },
    // User Data (Access=0xF2, Flags=0x00)
    { .limit0 = 0, .base0 = 0, .base1 = 0, .access = 0xF2, .flags_limit1 = 0x00, .base2 = 0 },
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

extern XTThread* currentThread;

const char* registers[] = {
    "rax", "rbx", "rcx", "rdx",
    "rbp", "rsi", "rdi",
    "r8", "r9", "r10", "r11",
    "r12", "r13", "r14", "r15",
    "int", "error", "rip", "cs",
    "rflags", "rsp", "ss"
};

#include <xt/result.h>
#include <stdint.h>

/* Частота PIT в Гц (10 мс = 100 Гц) */
#define PIT_FREQUENCY_HZ 100

/* Векторы прерываний для PIC */
#define PIC1_OFFSET 0x20  // Master PIC map to int 32 (0x20)
#define PIC2_OFFSET 0x28  // Slave PIC map to int 40 (0x28)

/* Инициализация PIC (8259A) и ремаппинг прерываний */
XTResult xtInitializePIC(void);

/* Инициализация PIT (8253/8254) на заданную частоту */
XTResult xtInitializePIT(void);

/* Вспомогательная функция для маскирования IRQ (опционально) */
XTResult xtMaskIRQ(uint8_t irq);
XTResult xtUnmaskIRQ(uint8_t irq);
#include <xt/arch/x86_64.h>
#include <xt/result.h>

/* Порты PIC */
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

/* Команды PIC */
#define ICW1_ICW4       0x01        /* ICW4 (not) needed */
#define ICW1_SINGLE     0x02        /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04        /* Call address interval 4 */
#define ICW1_LEVEL      0x08        /* Level triggered (edge) mode */
#define ICW1_INIT       0x10        /* Initialization - required! */

#define ICW4_8086       0x01        /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02        /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08        /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C        /* Buffered mode/master */
#define ICW4_SFNM       0x10        /* Special fully nested (not) */

/* Порты PIT */
#define PIT_CHANNEL0    0x40
#define PIT_COMMAND     0x43
#define PIT_BASE_FREQ   1193182

/* Небольшая задержка для старого железа при работе с IO */
static XTResult io_wait(void) {
    // Запись в неиспользуемый порт (обычно 0x80 используется для POST кодов)
    // заставляет CPU подождать несколько микросекунд
    return xtOutB(0x80, 0); 
}

XTResult xtInitializePIC(void) {
    uint8_t a1, a2;

    // Сохраним текущие маски (хотя мы их перепишем ниже, но для корректности чтения)
    XT_TRY(xtInB(PIC1_DATA, &a1));
    XT_TRY(xtInB(PIC2_DATA, &a2));

    // ICW1: Запуск инициализации (в каскадном режиме)
    XT_TRY(xtOutB(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4));
    XT_TRY(io_wait());
    XT_TRY(xtOutB(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4));
    XT_TRY(io_wait());

    // ICW2: Вектора прерываний (Master -> 0x20, Slave -> 0x28)
    XT_TRY(xtOutB(PIC1_DATA, PIC1_OFFSET));
    XT_TRY(io_wait());
    XT_TRY(xtOutB(PIC2_DATA, PIC2_OFFSET));
    XT_TRY(io_wait());

    // ICW3: Настройка каскадирования
    // Master PIC: бит 2 (4) указывает, что Slave подключен к IRQ2
    XT_TRY(xtOutB(PIC1_DATA, 4));
    XT_TRY(io_wait());
    // Slave PIC: число 2 указывает, что он подключен к IRQ2 мастера
    XT_TRY(xtOutB(PIC2_DATA, 2));
    XT_TRY(io_wait());

    // ICW4: Режим 8086
    XT_TRY(xtOutB(PIC1_DATA, ICW4_8086));
    XT_TRY(io_wait());
    XT_TRY(xtOutB(PIC2_DATA, ICW4_8086));
    XT_TRY(io_wait());

    // Восстановление масок или установка дефолтных
    // Сейчас мы замаскируем ВСЕ прерывания, кроме таймера (IRQ0), 
    // чтобы случайно не получить прерывание до настройки IDT.
    // 0xFE = 1111 1110 (0-й бит = 0 -> размаскирован)
    XT_TRY(xtOutB(PIC1_DATA, 0xFE)); 
    XT_TRY(xtOutB(PIC2_DATA, 0xFF));

    return XT_SUCCESS;
}

XTResult xtInitializePIT(void) {
    // Целевая частота 100 Гц (10 мс)
    // Divisor = 1193182 / 100 = 11931
    uint32_t divisor = PIT_BASE_FREQ / PIT_FREQUENCY_HZ;
    
    // Проверка на переполнение 16-битного счетчика
    if (divisor > 65535) {
        return XT_INVALID_PARAMETER; 
    }

    // Отправка команды:
    // 00 (Channel 0) | 11 (Access lo/hi byte) | 011 (Mode 3 Square Wave) | 0 (Binary)
    // 0x36
    XT_TRY(xtOutB(PIT_COMMAND, 0x36));
    XT_TRY(io_wait());

    // Отправка делителя (Low byte, затем High byte)
    XT_TRY(xtOutB(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF)));
    XT_TRY(io_wait());
    XT_TRY(xtOutB(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF)));
    
    return XT_SUCCESS;
}

void xtHalt() {
    asm volatile("hlt;");
}

XTResult xtUnmaskIRQ(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    XT_TRY(xtInB(port, &value));
    value &= ~(1 << irq); // Сбросить бит, чтобы размаскировать
    XT_TRY(xtOutB(port, value));

    return XT_SUCCESS;
}

XTResult xtMaskIRQ(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    XT_TRY(xtInB(port, &value));
    value |= (1 << irq); // Установить бит, чтобы замаскировать
    XT_TRY(xtOutB(port, value));

    return XT_SUCCESS;
}

void xtRegDump() {
    XTContext* context = currentThread->context;
    
    for (uint64_t i = 0; i < sizeof(registers)/sizeof(*registers);++i) {
        xtDebugPrint("% 6s=0x%016llx ", registers[i], ((uint64_t*)context)[i]);
        if (i % 4 == 0 && i != 0) xtDebugPrint("\n");
    }

}

/* Реализация в pic_pit.c */
#define PIC_EOI         0x20

XTResult xtPicSendEOI(uint8_t irq) {
    // Если прерывание от Slave PIC (IRQ >= 8), нужно уведомить Slave
    if (irq >= 8) {
        XT_TRY(xtOutB(PIC2_COMMAND, PIC_EOI));
    }

    // Master PIC уведомляем в любом случае (даже если пришло от Slave)
    XT_TRY(xtOutB(PIC1_COMMAND, PIC_EOI));

    return XT_SUCCESS;
}

void xtSchedule();

void xtExceptionHandler() {

    if (currentThread->context->interruptNumber == 0x20) {
        xtSchedule();
        xtPicSendEOI(0);
    }
    else {
        xtRegDump();
        while(1);
    }
}

XTResult xtSetIRQ(uint8_t vector, PFNXTIRQFunc irqFunc) {
    idt[vector].flags = 0x8e00;
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

XTResult xtDTInit() {
    // Важно: LGDT просто загружает указатель. 
    // Чтобы изменения вступили в силу, нужно перезагрузить сегментные регистры.
    
    xtAllocatePages(NULL, 0x1000, &idt);

    asm volatile("lgdt %0" : : "m"(_gdtr));
    idtr.addr = idt;
    idtr.size = 0x1000 - 1;
    asm volatile("lidt %0" :: "m"(idtr));
    
    xtSetIRQ(0, isr_stub_0);
    xtSetIRQ(1, isr_stub_1);
    xtSetIRQ(2, isr_stub_2);
    xtSetIRQ(3, isr_stub_3);
    xtSetIRQ(4, isr_stub_4);
    xtSetIRQ(5, isr_stub_5);
    xtSetIRQ(6, isr_stub_6);
    xtSetIRQ(7, isr_stub_7);
    xtSetIRQ(8, isr_stub_8);
    xtSetIRQ(9, isr_stub_9);
    xtSetIRQ(10, isr_stub_10);
    xtSetIRQ(11, isr_stub_11);
    xtSetIRQ(12, isr_stub_12);
    xtSetIRQ(13, isr_stub_13);
    xtSetIRQ(14, isr_stub_14);
    xtSetIRQ(15, isr_stub_15);
    xtSetIRQ(16, isr_stub_16);
    xtSetIRQ(17, isr_stub_17);
    xtSetIRQ(18, isr_stub_18);
    xtSetIRQ(19, isr_stub_19);
    xtSetIRQ(20, isr_stub_20);
    xtSetIRQ(21, isr_stub_21);
    xtSetIRQ(22, isr_stub_22);
    xtSetIRQ(23, isr_stub_23);
    xtSetIRQ(24, isr_stub_24);
    xtSetIRQ(25, isr_stub_25);
    xtSetIRQ(26, isr_stub_26);
    xtSetIRQ(27, isr_stub_27);
    xtSetIRQ(28, isr_stub_28);
    xtSetIRQ(29, isr_stub_29);
    xtSetIRQ(30, isr_stub_30);
    xtSetIRQ(31, isr_stub_31);
    xtSetIRQ(32, isr_stub_32);
    xtSetIRQ(40, isr_stub_40);

    xtInitializePIC();
    xtInitializePIT();


    // После lgdt нужно сделать "far return" или "far jump" для обновления CS
    // И обновить селекторы данных (ds, es, ss)
    return XT_SUCCESS;
}