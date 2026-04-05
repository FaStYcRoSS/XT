#include <xt/acpi.h>
#include <xt/scheduler.h>
#include <xt/kernel.h>

FADT* fadt = NULL;

XTResult xtArchACPIInit() {
    return xtFindACPITable("FACP", &fadt);
}

void write_to_gas(GenericAddressStructure *gas, uint16_t value) {
    xtDebugPrint("as %u gas->address 0x%llx value 0x%x\n", gas->AddressSpace, gas->Address, value);
    if (gas->AddressSpace == 0) { // System Memory
        volatile uint16_t *ptr = (volatile uint16_t*)(HIGHER_HALF_MEM(gas->Address));
        *ptr = value;
    } else if (gas->AddressSpace == 1) { // System I/O
        if (gas->Address <= 0xFFFF) {
            xtOutW(gas->Address, value);
        } else {
            xtDebugPrint("Invalid I/O address: 0x%llx, skipping\n", gas->Address);
        }
    }
    // другие типы (PCI, SMBus) игнорируем или обрабатываем отдельно
}

XTResult xtPowerOff() {
    if (fadt == NULL) return XT_NOT_IMPLEMENTED;
    uint16_t value = (1 << 13); // SLP_EN бит 13
    if (fadt->X_PM1aControlBlock.Address != 0) {
        write_to_gas(&fadt->X_PM1aControlBlock, value);
    } else if (fadt->PM1aControlBlock) {
        xtDebugPrint("gas->address 0x%x value 0x%x", fadt->PM1aControlBlock, value);
        xtOutW(fadt->PM1aControlBlock, value);
    }

    if (fadt->X_PM1bControlBlock.Address != 0 &&
        !(fadt->X_PM1bControlBlock.AddressSpace == 1 && fadt->X_PM1bControlBlock.Address > 0xFFFF)) {
        write_to_gas(&fadt->X_PM1bControlBlock, value);
    } else if (fadt->PM1bControlBlock != 0 && fadt->PM1bControlBlock <= 0xFFFF) {
        xtOutW(fadt->PM1bControlBlock, value);
    }
    while (1) asm volatile("hlt");
}

XTResult xtReboot() {
    if (fadt->ResetReg.Address != 0) {
        write_to_gas(&fadt->ResetReg, fadt->ResetValue);
        while (1) asm volatile("hlt");
    }
    return XT_NOT_IMPLEMENTED;
}

XTResult xtShutdown(uint64_t state) {
    if (state == XT_SHUTDOWN_POWER_OFF) {
        return xtPowerOff();
    }
    if (state == XT_SHUTDOWN_REBOOT) {
        return xtReboot();
    }
    return XT_NOT_IMPLEMENTED;
}