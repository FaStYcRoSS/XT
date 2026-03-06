#include <xt/memory.h>
#include <xt/pe.h>
#include <xt/kernel.h>
#include <xt/linker.h>
#include <xt/string.h>
#include <xt/sharedPtr.h>


typedef void(*PFNXTFunc)(void);

XTResult ApplyRelocations(void* physImage, void* virtualImage, uint64_t preferredBase) {
    PIMAGE_DOS_HEADER dos = physImage;
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)((uint8_t*)physImage + dos->e_lfanew);
    
    uint64_t relocAddr = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
    if (!relocAddr) return XT_SUCCESS;

    PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)((uint8_t*)physImage + relocAddr);
    int64_t delta = (int64_t)virtualImage - preferredBase;

    while (reloc->VirtualAddress != 0) {
        uint32_t count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(uint16_t);
        uint16_t* entry = (uint16_t*)(reloc + 1);

        for (uint32_t i = 0; i < count; i++) {
            int type = entry[i] >> 12;
            int offset = entry[i] & 0xFFF;
            if (type == 10) { // IMAGE_REL_BASED_DIR64
                uint64_t* patch = (uint64_t*)((uint8_t*)physImage + reloc->VirtualAddress + offset);
                *patch += delta;
            }
        }
        reloc = (PIMAGE_BASE_RELOCATION)((uint8_t*)reloc + reloc->SizeOfBlock);
    }
    return XT_SUCCESS;
}

typedef struct XTModule {
    const char* filename;
    void* physicalImage;
    void* virtualImage;
} XTModule; 

typedef struct XTModuleInstance {
    XTSharedPtr* physicalModulePtr; // Ссылка на XTModule
    void* virtualBase;              // Куда загружен в процессе
    const char* name;
} XTModuleInstance;

XTList* modules = NULL;

int xtIsDllFile(void* imageBase) {
    PIMAGE_DOS_HEADER dos = imageBase;
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)((uint8_t*)imageBase + dos->e_lfanew);
    return nt->FileHeader.Characteristics & IMAGE_FILE_DLL;
}

XTResult xtFindModuleList(const char* filename, XTList** list) {
    for (XTList* i = modules; i; xtGetNextList(i, &i)) {
        XTSharedPtr* sharedPtr = NULL;
        xtGetListData(i, &sharedPtr);
        XTModule* module = NULL;
        xtSharedPtrGetData(sharedPtr, &module);
        if (xtStringCmp(module->filename, filename, 4096) == 0) {
            *list = i;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}

XTResult xtRemoveModule(const char* filename) {
    XTList* list = NULL;
    XTResult result = xtFindModuleList(filename, &list);
    if (XT_IS_ERROR(result)) return result;
    result = xtRemoveFromList(modules, list);
    return result;
}


XTResult xtFindModule(const char* filename, void** imageBase) {
    
    XTList* moduleList = NULL;
    XTResult result = xtFindModuleList(filename, &moduleList);
    if (XT_IS_ERROR(result)) return result; 
    XTModule* module = NULL;
    xtGetListData(moduleList, &module);
    *imageBase = module->physicalImage;
    return XT_SUCCESS;
}

XTResult xtGetEntryPoint(void* physImage, void* virtualImage, PFNXTFunc* out) {
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physImage + ((PIMAGE_DOS_HEADER)physImage)->e_lfanew);
    *out = (uint8_t*)virtualImage + nt->OptionalHeader.AddressOfEntryPoint;
    return XT_SUCCESS;
}

XTResult xtFreeModuleInternal(void* data) {
    if (!data) return XT_INVALID_PARAMETER;

    XTModule* mod = (XTModule*)data;

    // 1. Извлекаем размер образа из PE-заголовка, чтобы знать, сколько страниц освобождать
    // Мы используем HIGHER_MEM, так как физический образ мапится в верхнюю половину ядра
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)HIGHER_MEM(mod->physicalImage);
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((uint8_t*)dosHeader + dosHeader->e_lfanew);
    uint64_t imageSize = ntHeaders->OptionalHeader.SizeOfImage;

    if (xtIsDllFile(mod->physicalImage)) {
        PFNXTLIBRARYMAIN libMain = NULL;
        xtGetEntryPoint(mod->physicalImage, mod->virtualImage, &libMain);
        XTResult result = libMain(XT_LIBRARY_DETACH);
        if (XT_IS_ERROR(result)) {
            return result;
        }
    }

    // 2. Освобождаем физические страницы кода и данных модуля
    if (mod->physicalImage) {
        xtFreePages(mod->physicalImage, imageSize);
    }

    // 3. Освобождаем дублированную строку с именем файла
    if (mod->filename) {
        // Предполагаем наличие функции xtHeapFree или аналогичной для строк
        xtHeapFree((void*)mod->filename);
    }

    // 4. Удаляем модуль из глобального списка кэша, чтобы его нельзя было найти
    // Важно: это предотвращает ситуацию, когда процесс пытается получить доступ к удаленному модулю
    xtRemoveModule(mod->filename);

    // 5. Освобождаем саму структуру XTModule
    xtHeapFree(mod);

    return XT_SUCCESS;
}

XTResult xtGetOrLoadPhysicalModule(const char* filename, XTSharedPtr** outPtr) {
    // 1. Ищем в глобальном списке 'modules' (из linker.c)
    XTList* listEntry = NULL;
    if (xtFindModuleList(filename, &listEntry) == XT_SUCCESS) {
        XTModule* mod = NULL;
        xtGetListData(listEntry, &mod);
        *outPtr = (XTSharedPtr*)mod; // Предполагаем, что в списке храним указатели
        return xtIncrementReference(*outPtr); //
    }

    // 2. Если не нашли — читаем с диска (логика из твоего xtInsertModule)
    XTFile* file = NULL;
    XT_TRY(xtOpenFile(filename, XT_FILE_MODE_READ, &file));
    void* fileBase = NULL;
    uint64_t fileSize = 0;
    xtMapFile(file, 0, &fileSize, &fileBase);
    fileBase = HIGHER_MEM(fileBase);

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)fileBase + ((PIMAGE_DOS_HEADER)fileBase)->e_lfanew);
    void* physImage = NULL;
    xtAllocatePages(NULL, nt->OptionalHeader.SizeOfImage, &physImage);
    
    // Копируем заголовки и секции в "чистый" физический образ
    xtCopyMem(HIGHER_MEM(physImage), fileBase, nt->OptionalHeader.SizeOfHeaders);
    PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        xtCopyMem((uint8_t*)HIGHER_MEM(physImage) + sections[i].VirtualAddress,
                  (uint8_t*)fileBase + sections[i].PointerToRawData, sections[i].SizeOfRawData);
    }

    // 3. Создаем умный указатель
    XTModule* newMod = NULL; // Твоя структура XTModule
    xtHeapAlloc(sizeof(XTModule), &newMod);
    newMod->physicalImage = physImage;
    xtDuplicateString(filename, (char**)&newMod->filename);

    XTResult res = xtCreateSharedPtr(newMod, outPtr, (PFNXTDELETEFUNC)xtFreeModuleInternal);
    if (XT_IS_ERROR(res)) {
        return res;
    }
    XTList* newList = NULL;
    xtCreateList(*outPtr, &newList);
    // Добавляем в глобальный список для кэширования
    xtAppendList(modules, newList); 
    
    xtUnmapFile(file, 0, fileBase, fileSize);
    xtCloseFile(file);
    return res;
}




XTResult xtGetProcAddress(void* physImage, void* virtualImage, const char* funcName, PFNXTFunc* func) {
    PIMAGE_DOS_HEADER dosHeader = HIGHER_HALF_MEM(physImage);
    PIMAGE_NT_HEADERS64 ntHeaders = dosHeader->e_lfanew + (uint8_t*)HIGHER_HALF_MEM(physImage);
    PIMAGE_EXPORT_DIRECTORY exportDirectory = ntHeaders->OptionalHeader.DataDirectory[0x0].VirtualAddress + (uint8_t*)HIGHER_HALF_MEM(physImage);
    for (uint64_t i = 0; i < exportDirectory->NumberOfFunctions; ++i) {
        PIMAGE_EXPORT_ENTRY funcEntry = exportDirectory->AddressOfFunctions + (uint8_t*)HIGHER_HALF_MEM(physImage);
        PIMAGE_EXPORT_ENTRY     nameEntry = exportDirectory->AddressOfNames + (uint8_t*)HIGHER_HALF_MEM(physImage);
        uint64_t funcNameLen = 0;
        xtGetStringLength(funcName, &funcNameLen);
        if (xtCompareMemory(nameEntry[i].rvaBase + (const char*)HIGHER_HALF_MEM(physImage), funcName, funcNameLen) == 0) {
            *func = funcEntry[i].rvaAddress + (const char*)virtualImage;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}

XTResult xtLoadModuleEx(XTProcess* process, const char* filename, void** base, PFNXTFunc* main);


XTResult xtLoadSubmodules(XTProcess* process, void* physImage, void* virtualImage) {
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physImage + ((PIMAGE_DOS_HEADER)physImage)->e_lfanew);
    uint32_t importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRVA) return XT_SUCCESS;

    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t*)physImage + importRVA);

    while (importDesc->Name) {
        const char* dllName = (const char*)((uint8_t*)physImage + importDesc->Name);
        void* subModuleBase = NULL;
        PFNXTFunc entryPoint = NULL;
        // Рекурсивная загрузка
        xtLoadModuleEx(process, dllName, &subModuleBase, &entryPoint);

        PIMAGE_THUNK_DATA64 thunk = (PIMAGE_THUNK_DATA64)((uint8_t*)physImage + importDesc->FirstThunk);
        PIMAGE_THUNK_DATA64 origThunk = (PIMAGE_THUNK_DATA64)((uint8_t*)physImage + importDesc->OriginalFirstThunk);

        while (origThunk->u1.AddressOfData) {
            if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                PIMAGE_IMPORT_BY_NAME importName = (PIMAGE_IMPORT_BY_NAME)((uint8_t*)physImage + origThunk->u1.AddressOfData);
                PFNXTFunc funcPtr = NULL;
                xtGetProcAddress(subModuleBase, subModuleBase, (const char*)importName->Name, &funcPtr);
                thunk->u1.Function = (uint64_t)funcPtr;
            }
            thunk++;
            origThunk++;
        }

        PFNXTLIBRARYMAIN libMain = (PFNXTLIBRARYMAIN)entryPoint;
        XTResult result = libMain(XT_LIBRARY_ATTACH);
        importDesc++;
    }
    return XT_SUCCESS;
}

XTResult xtLoadKernelSubmodules(void* physImage, void* virtualImage) {
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physImage + ((PIMAGE_DOS_HEADER)physImage)->e_lfanew);
    uint32_t importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRVA) return XT_SUCCESS;

    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t*)physImage + importRVA);

    while (importDesc->Name) {
        const char* dllName = (const char*)((uint8_t*)physImage + importDesc->Name);
        void* subModuleBase = NULL;
        
        // Рекурсивная загрузка
        XTSharedPtr* subModuleShared = NULL;
        xtGetOrLoadPhysicalModule(dllName, &subModuleShared);
        XTModule* mod = NULL;
        xtSharedPtrGetData(subModuleShared, &mod);
        subModuleBase = mod->physicalImage;

        PIMAGE_THUNK_DATA64 thunk = (PIMAGE_THUNK_DATA64)((uint8_t*)physImage + importDesc->FirstThunk);
        PIMAGE_THUNK_DATA64 origThunk = (PIMAGE_THUNK_DATA64)((uint8_t*)physImage + importDesc->OriginalFirstThunk);

        while (origThunk->u1.AddressOfData) {
            if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                PIMAGE_IMPORT_BY_NAME importName = (PIMAGE_IMPORT_BY_NAME)((uint8_t*)physImage + origThunk->u1.AddressOfData);
                PFNXTFunc funcPtr = NULL;
                xtGetProcAddress(subModuleBase, subModuleBase, (const char*)importName->Name, &funcPtr);
                thunk->u1.Function = (uint64_t)funcPtr;
            }
            thunk++;
            origThunk++;
        }
        PFNXTFunc entryPoint = NULL;
        xtGetEntryPoint(subModuleBase, subModuleBase, &entryPoint);
        PFNXTLIBRARYMAIN libMain = (PFNXTLIBRARYMAIN)entryPoint;
        XTResult result = libMain(XT_LIBRARY_ATTACH);
        importDesc++;
    }
    return XT_SUCCESS;
}


XTResult xtMapModuleToProcess(XTProcess* process, XTSharedPtr* physPtr, void* virtualBase) {
    XTModule* mod = NULL;
    xtSharedPtrGetData(physPtr, (void**)&mod);
    XT_CHECK_ARG_IS_NULL(mod);
    
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)HIGHER_MEM(mod->physicalImage) + ((PIMAGE_DOS_HEADER)HIGHER_MEM(mod->physicalImage))->e_lfanew);
    PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(nt);

    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        uint64_t attr = XT_MEM_USER;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) attr |= XT_MEM_EXEC;
        if (sections[i].Characteristics & IMAGE_SCN_MEM_READ)    attr |= XT_MEM_READ;
        
        void* sourcePhys = (uint8_t*)mod->physicalImage + sections[i].VirtualAddress;

        // CoW: Если секция допускает запись, создаем копию страницы
        if (sections[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            attr |= XT_MEM_WRITE;
            void* privateCopy = NULL;
            xtAllocatePages(NULL, (sections[i].SizeOfRawData + 0xFFF) & ~0xFFF, &privateCopy);
            xtCopyMem(HIGHER_MEM(privateCopy), HIGHER_MEM(sourcePhys), sections[i].SizeOfRawData);
            sourcePhys = privateCopy;
        }
        xtSetPages(process->pageTable, (uint64_t)virtualBase + sections[i].VirtualAddress, 
                   sourcePhys, (sections[i].SizeOfRawData + 0xFFF) & ~0xFFF, attr);
        
        xtInsertVirtualMap(process, (uint8_t*)virtualBase + sections[i].VirtualAddress, 
                           sourcePhys, (sections[i].SizeOfRawData + 0xFFF) & ~0xFFF, attr | XT_MEM_RESERVED);
    }
    return XT_SUCCESS;
}

XTResult xtLoadModuleEx(XTProcess* process, const char* filename, void** base, PFNXTFunc* f) {
    XTSharedPtr* physPtr = NULL;
    XT_TRY(xtGetOrLoadPhysicalModule(filename, &physPtr));

    XTModule* mod = NULL;
    xtSharedPtrGetData(physPtr, (void**)&mod);

        
    uint64_t moduleAslr = 0;
    xtGetRandomU64(&moduleAslr);
    moduleAslr = (0x00007f0000000000) + ((moduleAslr & ((1ull << 26) - 1)) << 12);
    // Определяем адрес загрузки (здесь можно добавить ASLR как в xtExecuteProgram)
    void* virtualBase = (void*)moduleAslr; // Упрощенно

    // 1. Маппим страницы в процесс
    xtMapModuleToProcess(process, physPtr, virtualBase);

    // 2. Рекурсивно грузим зависимости
    xtLoadSubmodules(process, HIGHER_MEM(mod->physicalImage), virtualBase);

    // 3. Применяем релокации ПРЯМО В МАППИНГ процесса 
    // (Поскольку страницы данных мы скопировали в CoW, мы не испортим оригинал)
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)HIGHER_MEM(mod->physicalImage) + ((PIMAGE_DOS_HEADER)HIGHER_MEM(mod->physicalImage))->e_lfanew);
    ApplyRelocations(HIGHER_MEM(mod->physicalImage), virtualBase, nt->OptionalHeader.ImageBase);

    *base = virtualBase;
    xtGetEntryPoint(HIGHER_MEM(mod->physicalImage), virtualBase, f);
    return XT_SUCCESS;
}

XTResult xtLoadKernelModule(const char* filename, PFNXTDRIVERMAIN* out) {
    XTSharedPtr* physPtr = NULL;
    XT_TRY(xtGetOrLoadPhysicalModule(filename, &physPtr));

    XTModule* mod = NULL;
    xtSharedPtrGetData(physPtr, (void**)&mod);
    void* physBase = HIGHER_MEM(mod->physicalImage);

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physBase + ((PIMAGE_DOS_HEADER)physBase)->e_lfanew);
    xtLoadKernelSubmodules(HIGHER_MEM(mod->physicalImage), HIGHER_MEM(mod->physicalImage));

    // Для ядра virtualBase == physicalBase (или в соответствии с твоим HIGHER_HALF_MEM)
    ApplyRelocations(physBase, physBase, nt->OptionalHeader.ImageBase);
    
    *out = (PFNXTDRIVERMAIN)((uint8_t*)physBase + nt->OptionalHeader.AddressOfEntryPoint);
    return XT_SUCCESS;
}

XTResult xtPassArgs(
    XTProcess* process,
    const char** args,
    void** out_argv
) {
    uint64_t str_va_base = 0;
    uint64_t argv_va_base = 0;
    int argc = 0;
    uint64_t strtabsize = 0;

    // 1. Считаем количество и общий размер строк
    for (const char** i = args; *i; ++i, ++argc) {
        uint64_t strsize = 0;
        xtGetStringLength(*i, &strsize);
        strtabsize += strsize + 1;
    }

    // 2. Выделяем и мапим таблицу строк (strtab)
    strtabsize = (strtabsize + 0xFFF) & ~0xFFF;
    xtGetRandomU64(&str_va_base);
    str_va_base = ((str_va_base & ((1ull << 34) - 1)) << 12);

    void* phys_strtab = NULL;
    xtAllocatePages(NULL, strtabsize, &phys_strtab);
    xtSetPages(process->pageTable, str_va_base, phys_strtab, strtabsize, XT_MEM_USER | XT_MEM_READ);
    xtInsertVirtualMap(process, (void*)str_va_base, phys_strtab, strtabsize, XT_MEM_USER | XT_MEM_READ | XT_MEM_RESERVED | XT_MEM_WRITE);

    // 3. Выделяем и мапим массив указателей (argv)
    // ВАЖНО: argc + 1 для NULL-терминатора
    uint64_t argv_size = (((argc + 1) * sizeof(char*)) + 0xFFF) & ~0xFFF;
    xtGetRandomU64(&argv_va_base);
    argv_va_base = ((argv_va_base & ((1ull << 34) - 1)) << 12);

    void* phys_argv = NULL;
    xtAllocatePages(NULL, argv_size, &phys_argv);
    xtSetPages(process->pageTable, argv_va_base, phys_argv, argv_size, XT_MEM_USER | XT_MEM_READ);
    xtInsertVirtualMap(process, (void*)argv_va_base, phys_argv, argv_size, XT_MEM_USER | XT_MEM_READ | XT_MEM_RESERVED | XT_MEM_WRITE);

    // 4. Копируем данные через "окно" в ядре
    uint8_t* k_strtab = (uint8_t*)HIGHER_MEM(phys_strtab);
    uint64_t** k_argv = (uint64_t**)HIGHER_MEM(phys_argv);
    
    uint64_t current_str_va = str_va_base;

    for (int i = 0; i < argc; i++) {
        uint64_t strsize = 0;
        xtGetStringLength(args[i], &strsize);
        
        // Копируем саму строку
        xtCopyMem(k_strtab, args[i], strsize + 1);
        
        // Записываем ВИРТУАЛЬНЫЙ адрес строки в массив argv
        k_argv[i] = (uint64_t*)current_str_va;
        
        k_strtab += strsize + 1;
        current_str_va += strsize + 1;
    }

    // 5. Ставим обязательный NULL в конце
    k_argv[argc] = NULL;

    *out_argv = (void*)argv_va_base;
    
    return XT_SUCCESS;
}
XTResult xtExecuteProgram(
    XTProcess* process,
    const char** args,
    const char** evnp
) {
    XTResult result = 0;
    PFNXTMAIN main = NULL;

    void* virtualBase = NULL;
    result = xtLoadModuleEx(process, args[0], &virtualBase, &main);
    if (XT_IS_ERROR(result)) {
        return result;
    }
    void* vargs = NULL;
    result = xtPassArgs(
        process,
        args,
        &vargs
    );
    if (XT_IS_ERROR(result)) {
        return result;
    }
    XTThread* thread = NULL;
    XT_TRY(xtCreateThread(
        process,
        (PFNXTTHREADFUNC)(main),
        0,
        vargs,
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

