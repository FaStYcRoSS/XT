#include <xt/acpi.h>
#include <xt/time.h>
#include <xt/kernel.h>

HPET* hpet = NULL;
uint64_t freq = 0;
uint64_t current_time_seconds = 0;
uint64_t current_time_nanoseconds = 0;
uint64_t lastGetTime = 0;
void* hpet_address = NULL;

void dump_hpet_table(HPET *hpet) {
    for (int i = 0; i < 56; i++) {
        xtDebugPrint("%02x ", ((uint8_t*)hpet)[i]);
        if ((i+1)%16 == 0) xtDebugPrint("\n");
    }
    xtDebugPrint("\n");
}

static volatile uint8_t *hpet_base;
static uint64_t hpet_period_fs;
static uint64_t last_hpet_ns;

XTResult xtHPETInit() {
    XT_TRY(xtFindACPITable("HPET", (void**)&hpet));
    if (hpet->address.AddressSpace != GAS_SPACE_SYSTEM_MEMORY || hpet->address.Address == 0)
        return XT_INVALID_INITIALIZATION;

    hpet_base = (volatile uint8_t*)HIGHER_HALF_MEM(hpet->address.Address);
    // Проверка доступности
    uint64_t caps = *(volatile uint64_t*)hpet_base;
    if (caps == 0xFFFFFFFFFFFFFFFF) // маркер ошибки
        return XT_ACCESS_VIOLATION;

    // Включить HPET
    volatile uint32_t *cfg = (volatile uint32_t*)(hpet_base + 0x10);
    if (!(*cfg & 1)) *cfg |= 1;

    hpet_period_fs = caps >> 32;
    if (hpet_period_fs == 0) return XT_INVALID_INITIALIZATION;

    return XT_SUCCESS;
}

XTResult XTEXPORT xtSetTime(uint64_t unixtime, uint64_t nanoseconds) {
    current_time_seconds = unixtime;
    current_time_nanoseconds = nanoseconds;
    uint64_t ticks = *(volatile uint64_t*)(hpet_base + 0xF0);
    last_hpet_ns = (ticks * hpet_period_fs) / 1000000;
    return XT_SUCCESS;
}

XTResult XTEXPORT xtGetTime(uint64_t* unixtime, uint64_t* nanoseconds) {
    uint64_t ticks = *(volatile uint64_t*)(hpet_base + 0xF0);
    uint64_t now_ns = (ticks * hpet_period_fs) / 1000000;
    uint64_t delta_ns = now_ns - last_hpet_ns;
    last_hpet_ns = now_ns;

    current_time_nanoseconds += delta_ns;
    if (current_time_nanoseconds >= 1000000000) {
        current_time_seconds += current_time_nanoseconds / 1000000000;
        current_time_nanoseconds %= 1000000000;
    }
    *unixtime = current_time_seconds;
    *nanoseconds = current_time_nanoseconds;
    return XT_SUCCESS;
}


