#include <xt/memory.h>
#include <xt/pe.h>
#include <xt/kernel.h>
#include <xt/linker.h>


XTResult xtStringCmp(const char* left, const char* right, uint64_t max);

XTResult xtLoadModule(
    XTProcess* process,
    const char* filename, 
    void* virtualBase, 
    uint64_t flags,
    PFNXTMAIN* out
) {
    XT_CHECK_ARG_IS_NULL(out);
    XT_CHECK_ARG_IS_NULL(process);
    PIMAGE_DOS_HEADER dosHeader = userProgram;
    PIMAGE_NT_HEADERS ntHeaders = (char*)userProgram + dosHeader->e_lfanew;
    void* physImage = 0;
    XT_TRY(xtAllocatePages(NULL, ntHeaders->OptionalHeader.SizeOfImage, &physImage));
    xtCopyMem(physImage, userProgram, ntHeaders->OptionalHeader.SizeOfHeaders);

    PIMAGE_SECTION_HEADER sections = (char*)ntHeaders + IMAGE_SIZEOF_NT_OPTIONAL64_HEADER + IMAGE_SIZEOF_FILE_HEADER + 4;
    for (uint64_t i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        xtCopyMem(physImage + sections[i].VirtualAddress, 
            (char*)userProgram + sections[i].PointerToRawData, sections[i].SizeOfRawData);
        
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
        xtSetPages(process->pageTable, 
            (uint64_t)virtualBase + sections[i].VirtualAddress, 
            physImage + sections[i].VirtualAddress, (sections[i].SizeOfRawData+0xfff) & (~(0xfff)), attr);
        
        void* dummy = NULL;

        xtInsertVirtualMap(
            process,
            (uint64_t)virtualBase + sections[i].VirtualAddress,
            physImage + sections[i].VirtualAddress,
            (sections[i].SizeOfRawData+0xfff) & (~(0xfff)),
            attr | XT_MEM_RESERVED
        );

    }
    *out = (uint64_t)virtualBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;
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

