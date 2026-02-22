#include <xt/acpi.h>
#include <xt/kernel.h>
#include <xt/string.h>

RSDP* rootACPI = NULL;

XTResult xtACPIInit() {

    EFI_GUID efiACPITable = ACPI_20_TABLE_GUID;

    for (
        uint64_t i = 0; 
        i < gKernelBootInfo->CountTables;
        ++i
    ) {
        EFI_CONFIGURATION_TABLE* table = (gKernelBootInfo->TablePtr) + i;
        if (xtCompareMemory(&table->VendorGuid, &efiACPITable, sizeof(EFI_GUID)) == XT_SUCCESS) {
            rootACPI = HIGHER_MEM(table->VendorTable);
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;

}

XTResult xtFindACPITable(const char* signature, void** out) {
    if (rootACPI == NULL) return XT_NOT_FOUND; // Проверка, что Init прошел успешно

    XSDT* xsdt = HIGHER_MEM(rootACPI->XsdtAddress);
    
    // Проверка на корректность длины, чтобы избежать underflow
    if (xsdt->Header.Length <= sizeof(DescriptionHeader)) {
        return XT_NOT_FOUND;
    }

    uint64_t entries = (xsdt->Header.Length - sizeof(DescriptionHeader)) / 8;
    for (uint64_t i = 0; i < entries; ++i) { // ОБЯЗАТЕЛЬНО инициализируем i = 0
        xtDebugPrint("table[%u] 0x%llx\n", i, xsdt->Tables[i]);
        DescriptionHeader* descHeader = HIGHER_MEM(xsdt->Tables[i]);
        if (descHeader == HIGHER_MEM(0)) continue;
        if (xtStringCmp(descHeader->Signature, signature, 4) == XT_SUCCESS) {
            *out = descHeader;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}