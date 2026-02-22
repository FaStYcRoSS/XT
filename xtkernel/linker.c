#include <xt/memory.h>
#include <xt/pe.h>
#include <xt/kernel.h>
#include <xt/linker.h>
#include <xt/string.h>

XTResult xtStringCmp(const char* left, const char* right, uint64_t max);

EFI_STATUS ApplyRelocations(void* physImage, void* virtualImage) {
    PIMAGE_DOS_HEADER dosHeader = physImage;
    PIMAGE_NT_HEADERS64 ntHeaders = dosHeader->e_lfanew + (uint8_t*)physImage;
    PIMAGE_BASE_RELOCATION reloc = 
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + (uint8_t*)physImage;
    int64_t AddBase = (int64_t)virtualImage - ntHeaders->OptionalHeader.ImageBase;
    for (PIMAGE_BASE_RELOCATION i = reloc; 
        i < (uint8_t*)reloc + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size; 
        i =(uint8_t*)i + i->SizeOfBlock) {
            PIMAGE_RELOCATION blocks = (uint8_t*)i + sizeof(IMAGE_BASE_RELOCATION);
        for (uint64_t j = 0; j < i->SizeOfBlock / 2; ++j) {
            WORD block = blocks[j].Type;
            if ((block >> 12) != 10) continue;
            int64_t* to_write = i->VirtualAddress + (uint8_t*)physImage + (block & 0xfff);
            *to_write += AddBase;
        }
    }

    return EFI_SUCCESS;

}

XTResult xtLoadModule(
    XTProcess* process,
    const char* filename, 
    void* virtualBase, 
    uint64_t flags,
    PFNXTMAIN* out
) {

    XT_CHECK_ARG_IS_NULL(out);
    XT_CHECK_ARG_IS_NULL(process);


    XTResult result = XT_SUCCESS;
    XTFile* input = NULL;
    result = xtOpenFile(filename, XT_FILE_MODE_READ, &input);
    if (XT_IS_ERROR(result)) {
        return result;
    }
    void* fileBase = NULL;
    uint64_t size = 0;
    result = xtMapFile(input, 0x0, &size, &fileBase);
    fileBase = HIGHER_MEM(fileBase);
    if (XT_IS_ERROR(result)) {
        xtCloseFile(input);
        return result;
    }


    PIMAGE_DOS_HEADER dosHeader = fileBase;
    PIMAGE_NT_HEADERS ntHeaders = (char*)fileBase + dosHeader->e_lfanew;
    void* physImage = 0;
    XT_TRY(xtAllocatePages(NULL, ntHeaders->OptionalHeader.SizeOfImage, &physImage));
    xtCopyMem(HIGHER_MEM(physImage), fileBase, ntHeaders->OptionalHeader.SizeOfHeaders);

    PIMAGE_SECTION_HEADER sections = (char*)ntHeaders + IMAGE_SIZEOF_NT_OPTIONAL64_HEADER + IMAGE_SIZEOF_FILE_HEADER + 4;
    for (uint64_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        xtCopyMem(HIGHER_MEM(physImage) + sections[i].VirtualAddress, 
            (char*)fileBase + sections[i].PointerToRawData, sections[i].SizeOfRawData);
        
        uint64_t attr = flags;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            attr |= XT_MEM_EXEC;
        }
        if (sections[i].Characteristics & IMAGE_SCN_MEM_READ) {
            attr |= XT_MEM_READ;
        }
        if (sections[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            attr |= XT_MEM_WRITE;
        }
        result = xtSetPages(process->pageTable, 
            (uint64_t)virtualBase + sections[i].VirtualAddress, 
            physImage + sections[i].VirtualAddress, (sections[i].SizeOfRawData+0xfff) & (~(0xfff)), attr);
        
        if (XT_IS_ERROR(result)) {
            xtFreePages(physImage, ntHeaders->OptionalHeader.SizeOfImage);
            return result;
        }

        void* dummy = NULL;

        result = xtInsertVirtualMap(
            process,
            (uint64_t)virtualBase + sections[i].VirtualAddress,
            physImage + sections[i].VirtualAddress,
            (sections[i].SizeOfRawData+0xfff) & (~(0xfff)),
            attr | XT_MEM_RESERVED
        );

        if (XT_IS_ERROR(result)) {
            xtFreePages(physImage, ntHeaders->OptionalHeader.SizeOfImage);
            return result;
        }

    }

    ApplyRelocations(HIGHER_MEM(physImage), virtualBase);
    *out = (uint64_t)virtualBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;
    return XT_SUCCESS;
}

XTResult xtExecuteProgram(
    XTProcess* process,
    const char** args,
    const char** evnp
) {
    XTResult result = 0;

    
    uint64_t moduleAslr = 0;
    xtGetRandomU64(&moduleAslr);
    moduleAslr = (0x00007f0000000000) + ((moduleAslr & ((1ull << 26) - 1)) << 12);

    PFNXTMAIN main = NULL;

    result = xtLoadModule(
        process,
        args[0],
        moduleAslr,
        XT_MEM_USER,
        &main
    );
    if (XT_IS_ERROR(result)) {
        return result;
    }
    int argc = 0;
    uint64_t strtabsize = 0;
    for (const char** i = args; *i; ++i, ++argc) {
        xtDebugPrint("arg[%u]=%s\n", argc, *i);
        uint64_t strsize = 0;
        xtGetStringLength(*i, &strsize);
        strtabsize += strsize + 1;
    }
    strtabsize = (strtabsize + ((1 << PAGE_SHIFT) - 1)) & (~((1 << PAGE_SHIFT) - 1)); //align
    xtGetRandomU64(&moduleAslr);
    moduleAslr = ((moduleAslr & ((1ull << 34) - 1)) << 12);

    void* strtab = NULL;
    xtAllocatePages(
        NULL,
        strtabsize,
        &strtab
    );
    xtSetPages(
        process->pageTable,
        moduleAslr,
        strtab,
        strtabsize,
        XT_MEM_USER | XT_MEM_READ | XT_MEM_RESERVED
    );

    result = xtInsertVirtualMap(
        process,
        moduleAslr,
        strtab,
        strtabsize,
        XT_MEM_USER | XT_MEM_READ | XT_MEM_RESERVED
    );

    uint64_t userArgsva = moduleAslr;
    xtGetRandomU64(&moduleAslr);
    moduleAslr = ((moduleAslr & ((1ull << 34) - 1)) << 12);
    void* userArgs = NULL;
    uint64_t userArgsSize = ((argc * 8) + ((1 << PAGE_SHIFT) - 1)) & (~((1 << PAGE_SHIFT) - 1));
    xtAllocatePages(
        NULL,
        userArgsSize,
        &userArgs
    );
    xtSetPages(
        process->pageTable,
        moduleAslr,
        userArgs,
        userArgsSize,
        XT_MEM_USER | XT_MEM_READ | XT_MEM_RESERVED
    );
    result = xtInsertVirtualMap(
        process,
        moduleAslr,
        userArgs,
        userArgsSize,
        XT_MEM_USER | XT_MEM_READ | XT_MEM_RESERVED
    );


    strtab = HIGHER_MEM(strtab);
    userArgs = HIGHER_MEM(userArgs);
    xtSetMem(strtab, 0, strtabsize);
    argc = 0;
    for (const char** i = args; *i; ++i, ++argc) {
        uint64_t strsize = 0;
        xtGetStringLength(*i, &strsize);
        xtCopyMem(strtab, *i, strsize+1);
        const char** strUserArgs = userArgs;
        strUserArgs[argc] = userArgsva;
        strtab = (uint8_t*)strtab + strsize + 1;
        userArgsva += strsize + 1;
    }

    XTThread* thread = NULL;
    XT_TRY(xtCreateThread(
        process,
        (PFNXTTHREADFUNC)(main),
        0,
        moduleAslr,
        XT_THREAD_USER | XT_THREAD_RUN_STATE,
        &thread
    ));
    return XT_SUCCESS;
}


XTResult xtFindSection(void* image, const char* sectionName, void** out) {

    XT_CHECK_ARG_IS_NULL(image);
    XT_CHECK_ARG_IS_NULL(sectionName);
    XT_CHECK_ARG_IS_NULL(out);

    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)image;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)(dosHeader->e_lfanew + (const char*)image);
    PIMAGE_SECTION_HEADER sections = (char*)ntHeaders + IMAGE_SIZEOF_NT_OPTIONAL64_HEADER + IMAGE_SIZEOF_FILE_HEADER + 4;
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        if (xtStringCmp(sections[i].Name, sectionName, 8) == XT_SUCCESS) {
            *out = sections[i].VirtualAddress + (char*)image;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}

