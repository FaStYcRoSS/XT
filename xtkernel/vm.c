#include <xt/scheduler.h>
#include <xt/memory.h>
#include <xt/kernel.h>

XTResult xtCreateVirtualMapEntry(
    void* virtualStart,
    void* physicalStart,
    uint64_t size,
    uint64_t attr,
    XTVirtualMap** out
) {

    XT_CHECK_ARG_IS_NULL(out);
    XTVirtualMap* mapEntry = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTVirtualMap), &mapEntry));
    mapEntry->attr = attr;
    mapEntry->size = size;
    mapEntry->physicalAddress = physicalStart;
    mapEntry->virtualStart = virtualStart;
    *out = mapEntry;
    return XT_SUCCESS;
}

XTResult xtInsertVirtualMap(
    XTProcess* process,
    void* virtualStart,
    void* physicalAddress,
    uint64_t size,
    uint64_t attr
) {
    XT_CHECK_ARG_IS_NULL(process);

    XTVirtualMap* mapEntry = NULL;
    XT_TRY(xtCreateVirtualMapEntry(virtualStart, physicalAddress, size, attr, &mapEntry));

    XTList* newNode = NULL;
    XT_TRY(xtCreateList(mapEntry, &newNode));

    // Случай 1: Список пуст
    if (process->memoryMap == NULL) {
        process->memoryMap = newNode;
        return XT_SUCCESS;
    }

    XTList* curr = process->memoryMap;
    XTList* prev = NULL;

    while (curr) {
        XTVirtualMap* existing = NULL;
        xtGetListData(curr, &existing);

        // Ищем место по порядку адресов (чтобы список был сортированным)
        if ((uint64_t)existing->virtualStart > (uint64_t)virtualStart) {
            if (prev == NULL) {
                // Случай 2: Вставка в самое начало (новый корень)
                // Нам нужно, чтобы newNode->next = process->memoryMap
                // В зависимости от реализации твоего xtList, это может быть:
                xtSetNextList(newNode, process->memoryMap); 
                process->memoryMap = newNode;
            } else {
                // Случай 3: Вставка в середину
                xtInsertList(prev, newNode);
            }
            return XT_SUCCESS;
        }

        prev = curr;
        xtGetNextList(curr, &curr);
    }

    // Случай 4: Вставка в самый конец
    xtAppendList(process->memoryMap, newNode);
    return XT_SUCCESS;
}



XTResult xtFindVirtualMap(
    XTProcess* process,
    void* ptr,
    XTVirtualMap** out,
    XTList** prevList
) {

    XT_CHECK_ARG_IS_NULL(process);
    XT_CHECK_ARG_IS_NULL(out);

    for (XTList* i = process->memoryMap, *prev = NULL; i; prev = i, xtGetNextList(i, &i)) {
        XTVirtualMap* map = NULL;
        xtGetListData(i, &map);
        if (map->virtualStart <= ptr && map->virtualStart + map->size >= ptr) {
            *out = map;
            if (prevList) {
                *prevList = prev;
            }
            return XT_SUCCESS;
        }
    }
    return XT_NOT_FOUND;
}

// XTResult xtAccessPtr(void* ptr, size_t size) {

//     XTProcess* currentProcess = NULL;
//     xtGetCurrentProcess(&currentProcess);
//     void* physPtr = NULL;
//     XTResult result = xtGetPhysicalAddress(currentProcess->pageTable, ptr, &physPtr);
//     if (result != XT_NOT_FOUND) {
//         return XT_SUCCESS;
//     }
//     XTVirtualMap* mapEntry = NULL;
//     size = ((((((uint64_t)ptr) & 0xfff) + size) >> PAGE_SHIFT) + 1) << PAGE_SHIFT;
//     ptr = ((uint64_t)ptr) & ~(0xfff);
//     XTList* prev = NULL;
//     XTResult result = xtFindVirtualMap(
//         currentProcess,
//         ptr,
//         &mapEntry,
//         &prev
//     );
//     if (result == XT_NOT_FOUND) {
//         return XT_NOT_FOUND;
//     }
//     void* newPage = NULL;
//     XT_TRY(xtAllocatePages(NULL, size, &newPage));
//     xtSetPages(
//         currentProcess->pageTable,
//         ptr,
//         newPage,
//         0x1000,
//         mapEntry->attr
//     );
//     xtInvalidatePage(ptr);
//     return XT_SUCCESS;
// }
