#ifndef __XT_EFI_H__
#define __XT_EFI_H__

#include <efi/efi.h>

EFI_STATUS EFIAPI EfiInitializeLib(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable);

extern EFI_SYSTEM_TABLE* gST;
extern EFI_HANDLE gImageHandle;
extern EFI_BOOT_SERVICES* gBS;
extern EFI_RUNTIME_SERVICES* gRT;

EFI_STATUS EFIAPI _EfiAssert(CHAR16* lineno, CHAR16* file, CHAR16* expr, EFI_STATUS status);

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)

#define STR(x) #x
#define INT_TO_STR(x) WIDEN(STR(x))
#define EfiAssert(x) _EfiAssert(INT_TO_STR(__LINE__), __WFILE__, INT_TO_STR(x), (x))

#define EfiTry(x) do {EFI_STATUS result = x; if (EFI_ERROR(result)) return result;} while(0)


EFI_STATUS EFIAPI EfiOutputString(CHAR16* str);
EFI_STATUS EfiPrintLn(CHAR16* str, ...);
EFI_STATUS EfiPrint(CHAR16* str, ...);

#define KERNEL_IMAGE_BASE 0xffffffff80000000
#define HIGH_HALF 0xffff800000000000
#define HIGHER_HALF_MEM(x) ((void*)((uint64_t)x | (HIGH_HALF)))

typedef struct KernelBootInfo {
    UINTN MemoryMapSize;
    UINTN DescSize;
    EFI_RUNTIME_SERVICES* RT;
    EFI_CONFIGURATION_TABLE* TablePtr;
    UINTN CountTables;
    void* initrd;
    uint64_t initrdSize;
    void* framebuffer;
    int32_t width, height;
    EFI_MEMORY_DESCRIPTOR descs[1];
} KernelBootInfo;

#endif

