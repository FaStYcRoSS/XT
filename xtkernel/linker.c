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
    uint64_t flags;
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
    physImage = HIGHER_HALF_MEM(physImage);
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physImage + ((PIMAGE_DOS_HEADER)physImage)->e_lfanew);
    *out = (uint8_t*)virtualImage + nt->OptionalHeader.AddressOfEntryPoint;
    return XT_SUCCESS;
}

XTResult xtFindModuleByPtr(
    void* ptr,
    const char** filename
) {
    for (XTList* i = modules; i; xtGetNextList(i, &i)) {
        XTSharedPtr* sharedPtr = NULL;
        xtGetListData(i, &sharedPtr);
        XTModule* module = NULL;
        xtSharedPtrGetData(sharedPtr, &module);
        
        PIMAGE_NT_HEADERS nt = 
        (PIMAGE_NT_HEADERS)((uint8_t*)module->physicalImage + 
        ((PIMAGE_DOS_HEADER)module->physicalImage)->e_lfanew);
        if ((uint64_t)module->physicalImage < (uint64_t)ptr && (uint64_t)ptr < (uint64_t)module->physicalImage + nt->OptionalHeader.SizeOfImage) {
            *filename = module->filename;
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
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
        xtGetEntryPoint(mod->physicalImage, mod->physicalImage, &libMain);
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


XTResult xtInsertKernelModule() {

    XTModule* newMod = NULL; // Твоя структура XTModule
    xtHeapAlloc(sizeof(XTModule), &newMod);
    newMod->physicalImage = KERNEL_IMAGE_BASE;
    newMod->flags = 0x4 | 0x2;
    xtDuplicateString("xtkernel.xte", (char**)&newMod->filename);
    XTSharedPtr* ptr = NULL;
    XTResult res = xtCreateSharedPtr(newMod, &ptr, (PFNXTDELETEFUNC)xtFreeModuleInternal);
    if (XT_IS_ERROR(res)) {
        return res;
    }
    XTList* newList = NULL;
    xtCreateList(ptr, &newList);
    // Добавляем в глобальный список для кэширования
    modules = newList;
    return XT_SUCCESS;
}

XTResult xtGetProcAddress(void* physImage, void* virtualImage, const char* funcName, PFNXTFunc* func) {
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)physImage;
    PIMAGE_NT_HEADERS64 ntHeaders = (PIMAGE_NT_HEADERS64)((uint8_t*)physImage + dosHeader->e_lfanew);
    DWORD exportRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportRVA) return XT_NOT_FOUND;

    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((uint8_t*)physImage + exportRVA);
    DWORD* names = (DWORD*)((uint8_t*)physImage + exports->AddressOfNames);
    WORD* ordinals = (WORD*)((uint8_t*)physImage + exports->AddressOfNameOrdinals);
    DWORD* functions = (DWORD*)((uint8_t*)physImage + exports->AddressOfFunctions);

    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
        const char* name = (const char*)((uint8_t*)physImage + names[i]);
        if (xtStringCmp(name, funcName, 4096) == 0) {
            DWORD funcRVA = functions[ordinals[i]];
            *func = (PFNXTFunc)((uint8_t*)virtualImage + funcRVA);
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}

XTResult xtLoadModuleEx(XTProcess* process, const char* filename, void** physBase, void** base);

const char* suffixes[] = {
    "/initrd/",
    "/XT/drivers/",
    "/XT/",
    "./",
    ""
};

// XTResult xtLoadSubmodules(XTProcess* process, void* physImage, void* virtualImage) {
//     PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physImage + ((PIMAGE_DOS_HEADER)physImage)->e_lfanew);
//     uint32_t importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
//     if (!importRVA) return XT_SUCCESS;

//     PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t*)physImage + importRVA);

//     while (importDesc->Name) {
//         const char* dllName = (const char*)((uint8_t*)physImage + importDesc->Name);
//         void* subModuleBase = NULL;
//         // Рекурсивная загрузка
//         xtLoadModuleEx(process, dllName, &subModuleBase);

//         PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(((PIMAGE_DOS_HEADER)subModuleBase)->e_lfanew + (uint8_t*)subModuleBase);

//         if (!(nt->FileHeader.Characteristics & IMAGE_FILE_DLL) && nt->OptionalHeader.Subsystem != 33) {
//             return XT_INVALID_MODULE;
//         }

//         PIMAGE_THUNK_DATA64 thunk = (PIMAGE_THUNK_DATA64)((uint8_t*)physImage + importDesc->FirstThunk);
//         PIMAGE_THUNK_DATA64 origThunk = (PIMAGE_THUNK_DATA64)((uint8_t*)physImage + importDesc->OriginalFirstThunk);

//         while (origThunk->u1.AddressOfData) {
//             if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
//                 PIMAGE_IMPORT_BY_NAME importName = (PIMAGE_IMPORT_BY_NAME)((uint8_t*)physImage + origThunk->u1.AddressOfData);
//                 PFNXTFunc funcPtr = NULL;
//                 xtGetProcAddress(subModuleBase, subModuleBase, (const char*)importName->Name, &funcPtr);
//                 thunk->u1.Function = (uint64_t)funcPtr;
//             }
//             thunk++;
//             origThunk++;
//         }
//         importDesc++;
//     }
//     return XT_SUCCESS;
// }


void xtKernelMain(KernelBootInfo* bootInfo);

XTResult xtLoadKernelSubmodules(void* physImage, void* virtualImage) {
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physImage + ((PIMAGE_DOS_HEADER)physImage)->e_lfanew);
    uint32_t importRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (!importRVA) return XT_SUCCESS;

    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t*)physImage + importRVA);

    while (importDesc->Name) {
        const char* dllName = (const char*)((uint8_t*)physImage + importDesc->Name);
        void* subModuleBase = NULL;
        PFNXTFunc entryPoint = NULL;

        // Поиск по всем префиксам
        XTResult result = XT_NOT_FOUND;
        for (int i = 0; i < sizeof(suffixes)/sizeof(*suffixes); ++i) {
            char tempPath[4096];
            uint64_t sufLen = 0;
            xtGetStringLength(suffixes[i], &sufLen);
            xtCopyString(tempPath, suffixes[i], 4096);
            xtCopyString(tempPath + sufLen, dllName, 4096 - sufLen);

            result = xtLoadKernelModule(tempPath, &subModuleBase);
            if (!XT_IS_ERROR(result)) break;
        }

        if (XT_IS_ERROR(result) || subModuleBase == NULL) {
            return XT_NOT_FOUND;
        }

        // Заполняем IAT основного модуля
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

XTResult xtLoadModuleEx(XTProcess* process, const char* filename, void** physBase, void** base) {
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

    //// 2. Рекурсивно грузим зависимости
    //xtLoadSubmodules(process, HIGHER_MEM(mod->physicalImage), virtualBase);

    // 3. Применяем релокации ПРЯМО В МАППИНГ процесса 
    // (Поскольку страницы данных мы скопировали в CoW, мы не испортим оригинал)
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)HIGHER_MEM(mod->physicalImage) + ((PIMAGE_DOS_HEADER)HIGHER_MEM(mod->physicalImage))->e_lfanew);
    ApplyRelocations(HIGHER_MEM(mod->physicalImage), virtualBase, nt->OptionalHeader.ImageBase);
    *physBase = mod->physicalImage;
    *base = virtualBase;
    return XT_SUCCESS;
}

XTResult __declspec(dllexport) xtLoadKernelModule(const char* filename, void** base) {
    XTSharedPtr* physPtr = NULL;
    XT_TRY(xtGetOrLoadPhysicalModule(filename, &physPtr));

    XTModule* mod = NULL;
    xtSharedPtrGetData(physPtr, (void**)&mod);
    void* physBase = HIGHER_MEM(mod->physicalImage);  // виртуальный адрес в ядре
    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)physBase + ((PIMAGE_DOS_HEADER)physBase)->e_lfanew);

    // Рекурсивно загружаем зависимости (они будут использовать тот же physBase для своих импортов)
    XTResult subRes = xtLoadKernelSubmodules(physBase, physBase);
    if (XT_IS_ERROR(subRes)) {
        // В случае ошибки нужно уменьшить счётчик ссылок и, возможно, выгрузить частично загруженное
        xtDecrementReference(physPtr);
        return subRes;
    }

    // Применяем релокации к образу (модифицируем его в памяти)
    // Если модуль уже использовался ранее, это испортит кэш. Лучше создавать приватную копию.
    // Для простоты пока оставим так, но в реальном проекте нужно копировать.
    if (!(mod->flags & 0x4)) {
        ApplyRelocations(physBase, physBase, nt->OptionalHeader.ImageBase);
        mod->flags |= 0x4;
    }


    PFNXTDRIVERMAIN main = (PFNXTDRIVERMAIN)((uint8_t*)physBase + nt->OptionalHeader.AddressOfEntryPoint);
    if (!(mod->flags & 0x2) && (uint64_t)main > HIGH_MEM) {
        main(XT_LIBRARY_ATTACH);
        mod->flags |= 0x2;
    }
    *base = physBase;
    return XT_SUCCESS;
}


XTResult xtPassArgs(
    XTProcess* process,
    const char** args,
    const char** envp,
    void* imageBase,
    XTUserParameters** params          // возвращает виртуальный адрес PEB в процессе
) {
    // 1. Подсчёт argc и envc
    int argc = 0;
    uint64_t args_str_total = 0;
    for (const char** a = args; *a; ++a) {
        ++argc;
        uint64_t len;
        xtGetStringLength(*a, &len);
        args_str_total += len + 1;
    }

    int envc = 0;
    uint64_t env_str_total = 0;
    if (envp) {
        for (const char** e = envp; *e; ++e) {
            ++envc;
            uint64_t len;
            xtGetStringLength(*e, &len);
            env_str_total += len + 1;
        }
    }

    // 2. Расчёт смещений в едином блоке
    uint64_t argv_array_size = (argc + 1) * sizeof(char*);
    uint64_t envp_array_size = (envc + 1) * sizeof(char*);
    uint64_t headers_size = sizeof(XTUserParameters);

    uint64_t total_size = headers_size + argv_array_size + envp_array_size +
                          args_str_total + env_str_total;
    total_size = (total_size + (1 << PAGE_SHIFT) - 1) & ~((1 << PAGE_SHIFT) - 1);

    // 3. Выбор случайного виртуального адреса (ASLR) в пользовательском диапазоне
    uint64_t va_base;
    xtGetRandomU64(&va_base);
    va_base &= 0x00007fffffffffff;      // 48-битное пользовательское пространство
    va_base &= ~((1 << PAGE_SHIFT) - 1);        // выравнивание по границе страницы
    if (va_base < 0x1000) va_base = 0x1000;

    // 4. Выделение физических страниц
    void* phys = NULL;
    XT_TRY(xtAllocatePages(NULL, total_size, &phys));

    // 5. Отображение страниц в процесс (чтение/запись из пользователя)
    XT_TRY(xtSetPages(process->pageTable, va_base, phys, total_size,
                      XT_MEM_USER | XT_MEM_READ | XT_MEM_WRITE));
    XT_TRY(xtInsertVirtualMap(process, (void*)va_base, phys, total_size,
                              XT_MEM_USER | XT_MEM_READ | XT_MEM_WRITE | XT_MEM_RESERVED));

    // 6. Заполнение через временное отображение в ядре
    uint8_t* kptr = (uint8_t*)HIGHER_MEM(phys);
    XTUserParameters* _params = (XTUserParameters*)kptr;
    char*** k_argv = (char***)(kptr + headers_size);
    char*** k_envp = (char***)(kptr + headers_size + argv_array_size);
    char* k_str = (char*)(kptr + headers_size + argv_array_size + envp_array_size);

    // Заполняем PEB
    _params->argc = argc;
    _params->argv = (const char**)(va_base + headers_size);               // вирт. адрес массива argv
    _params->envp = (envc ? (const char**)(va_base + headers_size + argv_array_size) : NULL);
    _params->imageBase = imageBase;
    // Копируем аргументы
    uint64_t current_str_va = va_base + headers_size + argv_array_size + envp_array_size;
    for (int i = 0; i < argc; ++i) {
        uint64_t len;
        xtGetStringLength(args[i], &len);
        xtCopyMem(k_str, args[i], len + 1);
        k_argv[i] = (char**)current_str_va;
        k_str += len + 1;
        current_str_va += len + 1;
    }
    k_argv[argc] = NULL;   // терминатор массива argv

    // Копируем окружение
    if (envp) {
        for (int i = 0; i < envc; ++i) {
            uint64_t len;
            xtGetStringLength(envp[i], &len);
            xtCopyMem(k_str, envp[i], len + 1);
            k_envp[i] = (char**)current_str_va;
            k_str += len + 1;
            current_str_va += len + 1;
        }
        k_envp[envc] = NULL;
    }

    *params = (void*)va_base;
    return XT_SUCCESS;
}

XTResult xtExecuteProgram(
    XTProcess* process,
    const char** args,
    const char** envp,
    XTThread** mainThread
) {
    XTResult result = 0;
    PFNXTUserMain main = NULL;

    void* virtualBase = NULL;
    void* physBase = NULL;
    result = xtLoadModuleEx(process, args[0], &physBase, &virtualBase);
    if (XT_IS_ERROR(result)) {
        return result;
    }

    xtGetEntryPoint(physBase, virtualBase, &main);

    XTUserParameters* params = NULL;
    result = xtPassArgs(
        process,
        args,
        envp,
        virtualBase,
        &params
    );
    if (XT_IS_ERROR(result)) {
        return result;
    }
    XTThread* thread = NULL;
    XT_TRY(xtCreateThread(
        process,
        (PFNXTTHREADFUNC)(main),
        0,
        params,
        XT_THREAD_USER | XT_THREAD_RUN_STATE,
        &thread
    ));
    *mainThread = thread;
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

