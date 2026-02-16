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
        xtDebugPrint("map->virtualStart 0x%llx map->size 0x%llx\n", map->virtualStart, map->size);
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
