#include <xt/efi.h>

#include <xt/pe.h>

typedef int(*KernelMainFunction)(KernelBootInfo* bootInfo);

EFI_STATUS EFIAPI EfiLoadKernel(
    EFI_FILE* root, 
    CHAR16* KernelPath, 
    void** _KernelImage,
    KernelMainFunction *kernelMain,
    UINTN* _SizeOfImage
) {
    EFI_FILE* kernelFile = NULL;

    EfiTry(root->Open(root, &kernelFile, KernelPath, EFI_FILE_MODE_READ, 0));


    void* headers = NULL;
    EfiTry(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&headers));

    UINTN BuffSize = 4096;

    kernelFile->Read(kernelFile, &BuffSize, headers);

    PIMAGE_DOS_HEADER dosHeader = headers;

    PIMAGE_NT_HEADERS ntHeaders = (char*)headers + dosHeader->e_lfanew;

    UINTN SizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

    if (_SizeOfImage)
        *_SizeOfImage = SizeOfImage;

    void* kernelImage = NULL;

    EfiTry(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, SizeOfImage / 4096, (EFI_PHYSICAL_ADDRESS*)&kernelImage));

    //EfiPrintLn(L"kernelImage 0x%llx\n", kernelImage);

    gBS->CopyMem(kernelImage, headers, 4096);
    PIMAGE_SECTION_HEADER sections = (char*)ntHeaders + IMAGE_SIZEOF_NT_OPTIONAL64_HEADER + IMAGE_SIZEOF_FILE_HEADER + 4;
    //EfiPrintLn(L"sections ptr: 0x%llx", sections);
    for (UINTN i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        if (sections[i].SizeOfRawData == 0) continue;
        BuffSize = sections[i].SizeOfRawData;
        void* offset = (char*)kernelImage + sections[i].VirtualAddress;
        EfiTry(kernelFile->SetPosition(kernelFile, sections[i].PointerToRawData));
        EfiTry(kernelFile->Read(kernelFile, &BuffSize, offset));
        //EfiPrintLn(L"%s: offset: 0x%llx SizeOfRawData: %u PositionToRawData 0x%llx", sections[i].Name, offset, sections[i].SizeOfRawData, sections[i].PointerToRawData);
    }

    if (_KernelImage != NULL)
        *_KernelImage = kernelImage;

    if (kernelMain != NULL)
        *kernelMain = ntHeaders->OptionalHeader.AddressOfEntryPoint;

    kernelFile->Close(kernelFile);

    gBS->FreePages(headers, 1);

    return EFI_SUCCESS;

}

typedef struct PageTable {
    uint64_t entries[512];
} PageTable;

#define GET_PAGE_TABLE(page_table, pos) (PageTable*)((page_table)->entries[pos] & (~(0xfff | (1ull << 63))))
#define SET_PAGE_TABLE(page_table, pos, entry, flags) ((page_table)->entries[pos] = (uint64_t)(entry) | flags)


extern void __stdcall entry_to_kernel(void* stack_ptr, KernelMainFunction mainFunc, KernelBootInfo* bootinfo);

//The entry point of our bootloader
EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* sysTable) {

    //open and read kernel from file

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* simpleFS = NULL;
    EFI_GUID simpleFSGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    EfiInitializeLib(ImageHandle, sysTable);

    EfiAssert(sysTable->BootServices->LocateProtocol(&simpleFSGuid, NULL, (void**)&simpleFS));

    EFI_FILE* root = NULL;
    EfiAssert(simpleFS->OpenVolume(simpleFS, &root));
    
    void* image = NULL;

    KernelMainFunction mainFunc = NULL;

    UINTN SizeOfImage = 0;

    EfiAssert(EfiLoadKernel(root, L"\\XT\\XTKERNEL.EXE", &image, &mainFunc, &SizeOfImage));

    EfiPrintLn(L"KernelImage: 0x%llx\n\rKernelMain: 0x%llx\n\r", image, mainFunc);
    //map to virtual high half

    PageTable* pml4 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&pml4));
    gBS->SetMem(pml4, 4096, 0);

    PageTable* pdp1 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&pdp1));
    gBS->SetMem(pdp1, 4096, 0);
    //addresses from 0xffff800000000000-0xffff807fffffffff
    for (UINTN i = 0; i < 512; ++i) {
        pdp1->entries[i] = (i << 30) | 0x83;
    }
    SET_PAGE_TABLE(pml4, 0, pdp1, 0x03);
    SET_PAGE_TABLE(pml4, 256, pdp1, 0x03);

    //addresses from 0xffffffff7ffe0000-0xfffffffffffffffff
    void* StackPtr = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 0x20, (EFI_PHYSICAL_ADDRESS*)&StackPtr));
    gBS->SetMem(StackPtr, 4096, 0);
    PageTable* pdp2 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&pdp2));
    gBS->SetMem(pdp2, 4096, 0);
    PageTable* ptStack = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&ptStack));
    gBS->SetMem(ptStack, 4096, 0);
    for (UINTN i = 0; i < 32; ++i) {
        SET_PAGE_TABLE(ptStack, 480+i, (char*)StackPtr+i*4096, 0x03);
    }

    PageTable* pd1 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1,(EFI_PHYSICAL_ADDRESS*) &pd1));
    gBS->SetMem(pd1, 4096, 0);
    SET_PAGE_TABLE(pd1, 511, ptStack, 0x03);
    SET_PAGE_TABLE(pdp2, 509, pd1, 0x03);

    PageTable* pd2 = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&pd2));
    gBS->SetMem(pd2, 4096, 0);
    PageTable* ptKernel = NULL;
    EfiAssert(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, (EFI_PHYSICAL_ADDRESS*)&ptKernel));
    gBS->SetMem(ptKernel, 4096, 0);
    for (UINTN i = 0; i < SizeOfImage / 4096; ++i) {
        SET_PAGE_TABLE(ptKernel, i, (char*)image+i*4096, 0x03);
    }
    SET_PAGE_TABLE(pd2, 0, ptKernel, 0x03);
    SET_PAGE_TABLE(pdp2, 510, pd2, 0x03);

    SET_PAGE_TABLE(pml4, 511, pdp2, 0x03);
    //memory map
    
    UINTN MemoryMapBuffSize = 0;
    UINTN MapKey = 0;
    UINTN DescSize = 0;
    UINT32 DescRevision = 0;


    gBS->GetMemoryMap(&MemoryMapBuffSize, NULL, &MapKey, &DescSize, &DescRevision);
    
    KernelBootInfo* bootInfo = NULL;
    EfiAssert(gBS->AllocatePool(EfiLoaderData, MemoryMapBuffSize+sizeof(KernelBootInfo), &bootInfo));
    EfiAssert(gBS->GetMemoryMap(&MemoryMapBuffSize, &bootInfo->descs[0], &MapKey, &DescSize, &DescRevision));
    bootInfo->CountTables = HIGHER_HALF_MEM(gST->NumberOfTableEntries);
    bootInfo->RT = HIGHER_HALF_MEM(gRT);
    bootInfo->MemoryMapSize = MemoryMapBuffSize;
    bootInfo->DescSize = DescSize;
    bootInfo->TablePtr = gST->ConfigurationTable;

    EfiAssert(gBS->ExitBootServices(ImageHandle, MapKey));

    //start kernel
    asm volatile("cli");
    asm volatile("mov %%rax, %%cr3;"::"a"(pml4));

    void* StackTop = (void*)(KERNEL_IMAGE_BASE-8);

    mainFunc = (uint64_t)mainFunc + KERNEL_IMAGE_BASE;

    entry_to_kernel(StackTop, mainFunc, bootInfo);

    while(1);
    return EFI_SUCCESS;
}