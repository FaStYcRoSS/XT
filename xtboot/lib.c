#include <xt/efi.h>

EFI_SYSTEM_TABLE* gST = NULL;
EFI_HANDLE gImageHandle = NULL;
EFI_BOOT_SERVICES* gBS = NULL;
EFI_RUNTIME_SERVICES* gRT = NULL;

EFI_STATUS EFIAPI EfiInitializeLib(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* ST) {
    gImageHandle = ImageHandle;
    gST = ST;
    gBS = gST->BootServices;
    gRT = gST->RuntimeServices;
    return EFI_SUCCESS;
}