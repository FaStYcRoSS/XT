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
    EfiTry(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, 1, (EFI_PHYSICAL_ADDRESS*)&headers));

    UINTN BuffSize = 4096;

    kernelFile->Read(kernelFile, &BuffSize, headers);

    PIMAGE_DOS_HEADER dosHeader = headers;

    PIMAGE_NT_HEADERS ntHeaders = (char*)headers + dosHeader->e_lfanew;

    UINTN SizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

    if (_SizeOfImage)
        *_SizeOfImage = SizeOfImage;

    void* kernelImage = NULL;

    EfiTry(gBS->AllocatePages(AllocateAnyPages, EfiRuntimeServicesData, SizeOfImage / 4096, (EFI_PHYSICAL_ADDRESS*)&kernelImage));
    gBS->SetMem(kernelImage, SizeOfImage, 0);

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


extern void __stdcall entry_to_kernel(void* stack_ptr, KernelMainFunction mainFunc, KernelBootInfo* bootinfo);

EFI_STATUS MapHighHalf(void* imageBase, UINTN SizeOfImage, void** pageTable);

EFI_STATUS SetPageTable(void* pageTable);

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

    EfiAssert(EfiLoadKernel(root, L"\\XT\\XTKERNEL.XTE", &image, &mainFunc, &SizeOfImage));

    //load initrd

    EFI_FILE* initrdFile = NULL;

    EfiAssert(root->Open(root, &initrdFile, L"\\XT\\XTINITRD", EFI_FILE_MODE_READ, 0));

    EFI_GUID fileInfoGUID = EFI_FILE_INFO_ID;

    UINTN BufferSize = 0;


    EFI_FILE_INFO* fileInfo = NULL;
    initrdFile->GetInfo(initrdFile, &fileInfoGUID, &BufferSize, NULL);
    EfiAssert(gBS->AllocatePool(EfiLoaderData, BufferSize, &fileInfo));


    initrdFile->GetInfo(initrdFile, &fileInfoGUID, &BufferSize, fileInfo);
    void* initrd = NULL;
    UINTN pagesCount = (fileInfo->FileSize + 0xfff) / 4096; // Так чуть чище
    EfiAssert(
        gBS->AllocatePages(
            AllocateAnyPages, 
            EfiRuntimeServicesData, 
            ((fileInfo->FileSize + 0xfff) & ~(0xfff))/4096,
             &initrd
        )
    );
    initrdFile->Read(initrdFile, &fileInfo->FileSize, initrd);

    EFI_GUID gopGUID = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EfiAssert(gBS->LocateProtocol(&gopGUID, NULL, (void**)&gop));
    void* framebuffer = gop->Mode->FrameBufferBase;
    int32_t width = gop->Mode->Info->HorizontalResolution, height = gop->Mode->Info->VerticalResolution;

    //map to virtual high half
    void* pageTable = NULL;
    MapHighHalf(image, SizeOfImage, &pageTable);
    
    //memory map
    
    UINTN MemoryMapBuffSize = 0;
    UINTN MapKey = 0;
    UINTN DescSize = 0;
    UINT32 DescRevision = 0;

    gBS->GetMemoryMap(&MemoryMapBuffSize, NULL, &MapKey, &DescSize, &DescRevision);
    
    KernelBootInfo* bootInfo = NULL;
    EfiAssert(gBS->AllocatePool(EfiRuntimeServicesData, MemoryMapBuffSize+sizeof(KernelBootInfo), &bootInfo));
    EfiAssert(gBS->GetMemoryMap(&MemoryMapBuffSize, &bootInfo->descs[0], &MapKey, &DescSize, &DescRevision));
    bootInfo->CountTables = HIGHER_HALF_MEM(gST->NumberOfTableEntries);
    bootInfo->RT = HIGHER_HALF_MEM(gRT);
    bootInfo->MemoryMapSize = MemoryMapBuffSize;
    bootInfo->DescSize = DescSize;
    bootInfo->TablePtr = gST->ConfigurationTable;
    bootInfo->initrd = HIGHER_HALF_MEM(initrd);
    bootInfo->framebuffer = HIGHER_HALF_MEM(framebuffer);
    bootInfo->width = width;
    bootInfo->height = height;
    bootInfo->initrdSize = fileInfo->FileSize;

    EfiAssert(gBS->ExitBootServices(ImageHandle, MapKey));

    //start kernel
    SetPageTable(pageTable);

    void* StackTop = (void*)(KERNEL_IMAGE_BASE-8);

    mainFunc = (uint64_t)mainFunc + KERNEL_IMAGE_BASE;

    entry_to_kernel(StackTop, mainFunc, bootInfo);

    while(1);
    return EFI_SUCCESS;
}