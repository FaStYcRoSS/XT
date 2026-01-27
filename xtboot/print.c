#include <xt/efi.h>
#include "printf.h"

void _putchar(char _ch) {

}

void ___chkstk_ms() {

}

EFI_STATUS EFIAPI EfiOutputString(CHAR16* str) {
    return gST->ConOut->OutputString(gST->ConOut, str);
}

EFI_STATUS EfiVPrint(CHAR16* str, va_list va) {

    unsigned short buffer[4096];

    vwsnprintf_(buffer, 4096, str, va);

    gST->ConOut->OutputString(gST->ConOut, buffer);
    return EFI_SUCCESS;

}


EFI_STATUS EfiPrint(CHAR16* str, ...) {

    va_list va;

    va_start(va, str);

    EfiVPrint(str, va);

    va_end(va);
    return EFI_SUCCESS;
}

EFI_STATUS EfiPrintLn(CHAR16* str, ...) {

    va_list va;

    va_start(va, str);

    EfiVPrint(str, va);
    gST->ConOut->OutputString(gST->ConOut, L"\n\r");

    va_end(va);
    return EFI_SUCCESS;
}

