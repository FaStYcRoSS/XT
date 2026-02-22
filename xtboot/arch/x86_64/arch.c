#include <xt/efi.h>

typedef struct PageTable {
    uint64_t entries[512];
} PageTable;

#define GET_PAGE_TABLE(page_table, pos) (PageTable*)((page_table)->entries[pos] & (~(0xfff | (1ull << 63))))
#define SET_PAGE_TABLE(page_table, pos, entry, flags) ((page_table)->entries[pos] = (uint64_t)(entry) | flags)

EFI_STATUS MapHighHalf(void* image, UINTN SizeOfImage, void** pageTable) {
    PageTable* pml4 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&pml4));
    gBS->SetMem(pml4, 4096, 0);

    PageTable* pdp1 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&pdp1));
    gBS->SetMem(pdp1, 4096, 0);
    //addresses from 0xffff800000000000-0xffff807fffffffff
    for (UINTN i = 0; i < 512; ++i) {
        pdp1->entries[i] = (i << 30) | 0x87;
    }
    SET_PAGE_TABLE(pml4, 0, pdp1, 0x07);
    SET_PAGE_TABLE(pml4, 256, pdp1, 0x03);

    //addresses from 0xffffffff7ffe0000-0xfffffffffffffffff
    void* StackPtr = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 0x20, (EFI_PHYSICAL_ADDRESS*)&StackPtr));
    gBS->SetMem(StackPtr, 4096, 0);
    PageTable* pdp2 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&pdp2));
    gBS->SetMem(pdp2, 4096, 0);
    PageTable* ptStack = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&ptStack));
    gBS->SetMem(ptStack, 4096, 0);
    for (UINTN i = 0; i < 32; ++i) {
        SET_PAGE_TABLE(ptStack, 480+i, (char*)StackPtr+i*4096, 0x03);
    }

    PageTable* pd1 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1,(EFI_PHYSICAL_ADDRESS*) &pd1));
    gBS->SetMem(pd1, 4096, 0);
    SET_PAGE_TABLE(pd1, 511, ptStack, 0x03);
    SET_PAGE_TABLE(pdp2, 509, pd1, 0x03);

    PageTable* pd2 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&pd2));
    gBS->SetMem(pd2, 4096, 0);
    PageTable* ptKernel = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&ptKernel));
    gBS->SetMem(ptKernel, 4096, 0);
    for (UINTN i = 0; i < SizeOfImage / 4096; ++i) {
        SET_PAGE_TABLE(ptKernel, i, (char*)image+i*4096, 0x03);
    }
    SET_PAGE_TABLE(pd2, 0, ptKernel, 0x03);
    SET_PAGE_TABLE(pdp2, 510, pd2, 0x03);

    SET_PAGE_TABLE(pml4, 511, pdp2, 0x03);

    *pageTable = pml4;

    return EFI_SUCCESS;

}

EFI_STATUS SetPageTable(void* pageTable) {
    asm volatile("cli");
    asm volatile("mov %%rax, %%cr3;"::"a"(pageTable));
    return EFI_SUCCESS;
}