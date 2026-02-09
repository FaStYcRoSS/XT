#include <xt/memory.h>
#include <xt/pe.h>
#include <xt/kernel.h>

XTResult xtStringCmp(const char* left, const char* right, uint64_t max);

XTResult xtFindSection(void* image, const char* sectionName, void** out) {

    XT_CHECK_ARG_IS_NULL(image);
    XT_CHECK_ARG_IS_NULL(sectionName);
    XT_CHECK_ARG_IS_NULL(out);

    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)image;
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)(dosHeader->e_lfanew + (const char*)image);
    PIMAGE_SECTION_HEADER sections = (char*)ntHeaders + IMAGE_SIZEOF_NT_OPTIONAL64_HEADER + IMAGE_SIZEOF_FILE_HEADER + 4;
    for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i) {
        xtDebugPrint("%s\n", sections[i].Name);
        if (xtStringCmp(sections[i].Name, sectionName, 8) == XT_SUCCESS) {
            *out = sections[i].VirtualAddress + (char*)image;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}

