#include <xt/arch/x86_64.h>

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
void xtSendEOI() {
    xtPicSendEOI(0);
}

XTResult xtPITInit() {
    xtInitializePIC();
    xtInitializePIT();

    return XT_SUCCESS;

}