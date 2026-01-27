#include <xt/efi.h>

static CHAR16* str_Error[] = {
    L"SUCCESS",
    L"LOAD_ERROR",
    L"INVALID_PARAMETER",
    L"UNSUPPORTED",
    L"BAD_BUFFER_SIZE",
    L"BUFFER_TOO_SMALL",
    L"NOT_READY",
    L"DEVICE_ERROR",
    L"WRITE_PROTECTED",
    L"OUT_OF_RESOURCES",
    L"VOLUME_CORRUPTED",
    L"VOLUME_FULL",
    L"NO_MEDIA",
    L"MEDIA_CHANGED",
    L"NOT_FOUND",
    L"ACCESS_DENIED",
    L"NO_RESPONSE",
    L"NO_MAPPING",
    L"TIMEOUT",
    L"NOT_STARTED",
    L"ALREADY_STARTED",
    L"ABORTED",
    L"ICMP_ERROR",
    L"TFTP_ERROR",
    L"PROTOCOL_ERROR",
    L"INCOMPATIBLE_VERSION",
    L"SECURITY_VIOLATION",
    L"CRC_ERROR",
    L"END_OF_MEDIA",
    L"END_OF_FILE",
    L"INVALID_LANGUAGE",
    L"COMPROMISED_DATA",
    L"IP_ADDRESS_CONFLICT",
    L"HTTP_ERROR"
};

EFI_STATUS EFIAPI _EfiAssert(CHAR16* lineno, CHAR16* file, CHAR16* expr, EFI_STATUS status) {

    if (!EFI_ERROR(status)) return EFI_SUCCESS;
    EfiOutputString(file);
    EfiOutputString(L":");
    EfiOutputString(lineno);
    EfiOutputString(L":");
    EfiOutputString(expr);
    EfiOutputString(L":");
    EfiOutputString(str_Error[status & ~(EFI_ERROR_MASK)]);
    EfiOutputString(L"\n\r");
    return EFI_SUCCESS;

}