#include <xt/memory.h>
#include <xt/kernel.h>
#include <xt/efi.h>
#include <xt/list.h>

static uint8_t heap_data[256 * 1024];
static char heap_initialized = 0;

typedef struct XTHeapEntry {
    uint64_t size;        // Размер всего блока (включая заголовок)
    uint8_t used;         // 1 = занят, 0 = свободен
    struct XTHeapEntry* next;
    struct XTHeapEntry* prev;
    // Данные начинаются здесь
} XTHeapEntry;

// Размер заголовка без выравнивания
#define HEAP_HEADER_SIZE (sizeof(XTHeapEntry))

// Минимальный размер выделяемого блока
#define MIN_BLOCK_SIZE (HEAP_HEADER_SIZE + 16)

// Начало кучи
static XTHeapEntry* heap_start = NULL;

// Инициализация кучи
XTResult xtHeapInit() {
    if (heap_initialized) return XT_SUCCESS;
    
    // Выравниваем начало кучи
    heap_start = (XTHeapEntry*)heap_data;
    uintptr_t aligned_start = (uintptr_t)heap_start;
    if (aligned_start % 16 != 0) {
        aligned_start = (aligned_start + 15) & ~15;
    }
    heap_start = (XTHeapEntry*)aligned_start;
    
    // Инициализируем первый блок как свободный
    uint64_t heap_size = sizeof(heap_data) - (aligned_start - (uintptr_t)heap_data);
    heap_size = heap_size & ~15;  // Выравниваем размер
    
    heap_start->size = heap_size;
    heap_start->used = 0;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    
    heap_initialized = 1;
    return XT_SUCCESS;
}

// Выделение памяти
XTResult xtHeapAlloc(uint64_t size, void** out) {
    XT_CHECK_ARG_IS_NULL(out);
    
    // Инициализируем кучу при первом вызове
    if (!heap_initialized) {
        XT_TRY(xtHeapInit());
    }
    
    // Выравнивание размера
    size = (size + 15) & ~15;
    
    // Минимальный размер блока
    uint64_t needed_size = size + HEAP_HEADER_SIZE;
    if (needed_size < MIN_BLOCK_SIZE) {
        needed_size = MIN_BLOCK_SIZE;
    }
    
    XTHeapEntry* current = heap_start;
    
    // Поиск подходящего свободного блока
    while (current != NULL) {
        if (!current->used && current->size >= needed_size) {
            // Можно ли разделить блок?
            if (current->size >= needed_size + MIN_BLOCK_SIZE) {
                // Создаем новый свободный блок
                XTHeapEntry* new_block = (XTHeapEntry*)((char*)current + needed_size);
                new_block->size = current->size - needed_size;
                new_block->used = 0;
                new_block->next = current->next;
                new_block->prev = current;
                
                // Обновляем связи соседей
                if (current->next != NULL) {
                    current->next->prev = new_block;
                }
                
                // Обновляем текущий блок
                current->size = needed_size;
                current->next = new_block;
            }
            
            // Помечаем блок как занятый
            current->used = 1;
            
            // Возвращаем указатель на данные
            *out = (void*)((char*)current + HEAP_HEADER_SIZE);
            
            return XT_SUCCESS;
        }
        
        current = current->next;
    }
    return XT_OUT_OF_MEMORY;
}

// Освобождение памяти
XTResult xtHeapFree(void* ptr) {
    XT_CHECK_ARG_IS_NULL(ptr);
    
    // Получаем заголовок блока
    XTHeapEntry* block = (XTHeapEntry*)((char*)ptr - HEAP_HEADER_SIZE);
    
    // Проверка на двойное освобождение
    if (!block->used) {
        return XT_INVALID_PARAMETER;
    }
    
    // Освобождаем блок
    block->used = 0;
    
    // Объединяем с предыдущим свободным блоком
    if (block->prev != NULL && !block->prev->used) {
        XTHeapEntry* prev = block->prev;
        prev->size += block->size;
        prev->next = block->next;
        
        if (block->next != NULL) {
            block->next->prev = prev;
        }
        
        block = prev;  // Продолжаем с объединенного блока
    }
    
    // Объединяем со следующим свободным блоком
    if (block->next != NULL && !block->next->used) {
        block->size += block->next->size;
        block->next = block->next->next;
        
        if (block->next != NULL) {
            block->next->prev = block;
        }
    }
    return XT_SUCCESS;
}

// Статистика кучи
XTResult xtHeapGetStats(uint64_t* total, uint64_t* used, uint64_t* free_blocks) {
    if (!heap_initialized) {
        xtHeapInit();
    }
    
    uint64_t total_mem = 0;
    uint64_t used_mem = 0;
    uint64_t free_count = 0;
    
    XTHeapEntry* current = heap_start;
    while (current != NULL) {
        total_mem += current->size;
        if (current->used) {
            used_mem += current->size;
        } else {
            free_count++;
        }
        current = current->next;
    }
    
    if (total) *total = total_mem;
    if (used) *used = used_mem;
    if (free_blocks) *free_blocks = free_count;
    
    return XT_SUCCESS;
}

// Ваши функции преобразования типов памяти
const char* MemoryTypeStr[] = {
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",
    "EfiUnusableMemory",
    "EfiACPIReclaimMemory",
    "EfiACPIMemoryNVS",
    "EfiMemoryMappedIO",
    "EfiMemoryMappedIOPortSpace",
    "EfiPalCode",
    "EfiPersistentMemory",
    "EfiUnacceptedMemoryType",
    "EfiMaxMemoryType"
};

uint64_t xtUEFITypeToXT(uint64_t type) {
    switch (type) {
        case EfiReservedMemoryType:
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiPalCode:
            return XT_MEM_RESERVED;
        case EfiConventionalMemory:
        case EfiBootServicesCode:
        case EfiBootServicesData:
            return XT_MEM_FREE;
        case EfiUnacceptedMemoryType:
        case EfiUnusableMemory:
            return XT_MEM_UNUSABLE;
        case EfiACPIMemoryNVS:
            return XT_MEM_NVS;
        case EfiACPIReclaimMemory:
            return XT_MEM_ACPI;
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
            return XT_MEM_MMIO;
        default:
            return XT_MEM_UNKNOWN;
    }
}

// Глобальный список дескрипторов памяти
XTList* memoryDescs = NULL;

typedef struct XTMemoryMapEntry {
    uint64_t attributes;
    uint64_t start;
    uint64_t length;
} XTMemoryMapEntry;

XTResult xtAllocatePages(void* ptr, uint64_t size, void** out) {

    //find first page with ptr.
    for (XTList* i = memoryDescs, *prev = NULL;i; prev=i, xtGetNextList(i, &i)) {
        XTMemoryMapEntry* mapEntry = NULL;
        xtGetListData(i, &mapEntry);
        if (mapEntry->start == (uint64_t)ptr && mapEntry->length >= size) {
            *out = mapEntry->start;
            mapEntry->start += size;
            mapEntry->length -= size;
            if (mapEntry->length == 0) {
                XTList* next = NULL;
                xtGetNextList(i, &next);
                if (prev) {
                    xtSetNextList(prev, next);
                }
                else {
                    memoryDescs = next;
                }
                xtDestroyList(i);
            }
            return XT_SUCCESS;
        }
    }

    //find anything (if ptr is NULL)
    for (XTList* i = memoryDescs, *prev = NULL;i; prev=i, xtGetNextList(i, &i)) {
        XTMemoryMapEntry* mapEntry = NULL;
        xtGetListData(i, &mapEntry);
        if (mapEntry->length >= size) {
            *out = mapEntry->start;
            mapEntry->start += size;
            mapEntry->length -= size;

            if (mapEntry->length == 0) {
                XTList* next = NULL;
                xtGetNextList(i, &next);
                if (prev) {
                    xtSetNextList(prev, next);
                }
                else {
                    memoryDescs = next;
                }
                xtDestroyList(i);
            }
            return XT_SUCCESS;
        }
    }
    return XT_OUT_OF_MEMORY;
}

XTResult xtFreePages(void* ptr, uint64_t size) {
    uint64_t addr = (uint64_t)ptr;
    XTList *curr = memoryDescs;
    XTList *prev = NULL;

    // 1. Ищем позицию для вставки (поддерживаем сортировку по адресу)
    while (curr) {
        XTMemoryMapEntry* entry = NULL;
        xtGetListData(curr, &entry);
        if (entry->start > addr) break; // Нашли место: новый блок должен быть перед curr
        
        prev = curr;
        xtGetNextList(curr, &curr);
    }

    // Теперь: prev — блок "слева" от нашего, curr — блок "справа"
    
    XTMemoryMapEntry* prevEntry = NULL;
    if (prev) xtGetListData(prev, &prevEntry);
    
    XTMemoryMapEntry* currEntry = NULL;
    if (curr) xtGetListData(curr, &currEntry);

    // 2. Попытка склеить с ПРЕДЫДУЩИМ блоком (слева)
    if (prevEntry && (prevEntry->start + prevEntry->length == addr)) {
        prevEntry->length += size;
        
        // А теперь проверим, не склеится ли этот расширенный prev со следующим (curr)
        if (currEntry && (prevEntry->start + prevEntry->length == currEntry->start)) {
            prevEntry->length += currEntry->length;
            
            // Выбрасываем curr из списка, так как он поглощен блоком prev
            XTList* nextAfterCurr = NULL;
            xtGetNextList(curr, &nextAfterCurr);
            xtSetNextList(prev, nextAfterCurr);
            xtDestroyList(curr);
        }
        return XT_SUCCESS;
    }

    // 3. Попытка склеить со СЛЕДУЮЩИМ блоком (справа), если со вторым не вышло
    if (currEntry && (addr + size == currEntry->start)) {
        currEntry->start = addr;
        currEntry->length += size;
        return XT_SUCCESS;
    }

    // 4. Склеить не удалось — создаем новый узел в списке
    XTMemoryMapEntry* newEntry = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTMemoryMapEntry), &newEntry));
    newEntry->start = addr;
    newEntry->length = size;
    newEntry->attributes = XT_MEM_FREE;

    XTList* newNode = NULL;
    xtCreateList(newEntry, &newNode);
    
    // Вставляем newNode между prev и curr
    xtSetNextList(newNode, curr);
    if (prev) {
        xtSetNextList(prev, newNode);
    } else {
        memoryDescs = newNode; // Вставка в самое начало
    }

    return XT_SUCCESS;
}

XTResult xtMemoryDump() {
    for (XTList* l = memoryDescs;l;xtGetNextList(l, &l)) {
        XTMemoryMapEntry* mapEntry = NULL;
        xtGetListData(l, &mapEntry);
        xtDebugPrint("0x%llx-0x%llx 0x%llx\n", 
            mapEntry->start, 
            mapEntry->length+mapEntry->start,
            mapEntry->attributes
        );
    }
    return XT_SUCCESS;
}

// Инициализация памяти
XTResult xtMemoryInit() {
    // Инициализируем кучу
    XT_TRY(xtHeapInit());

    // Сбрасываем список дескрипторов
    memoryDescs = NULL;
    
    EFI_MEMORY_DESCRIPTOR* desc = gKernelBootInfo->descs;
    uint8_t* end = (uint8_t*)gKernelBootInfo->descs + gKernelBootInfo->MemoryMapSize;
    
    XTMemoryMapEntry* last_entry = NULL;
    
    while ((uint8_t*)desc < end) {
        // Создаем новую запись для каждого дескриптора
        XTMemoryMapEntry* mapEntry = NULL;
        XTResult result = xtHeapAlloc(sizeof(XTMemoryMapEntry), (void**)&mapEntry);
        
        // Заполняем данные
        mapEntry->start = desc->PhysicalStart;
        mapEntry->length = desc->NumberOfPages * 4096;
        mapEntry->attributes = xtUEFITypeToXT(desc->Type);
        
        // Пытаемся объединить с предыдущей записью того же типа
        if (last_entry != NULL && 
            last_entry->attributes == mapEntry->attributes &&
            last_entry->start + last_entry->length == mapEntry->start) {
            
            // Объединяем регионы
            last_entry->length += mapEntry->length;
            
            // Освобождаем ненужную запись
            xtHeapFree(mapEntry);
        } else {
            // Создаем новый элемент списка
            XTList* new_node = NULL;
            XT_TRY(xtCreateList(mapEntry, &new_node));
            
            // Добавляем в список
            if (memoryDescs == NULL) {
                memoryDescs = new_node;
            } else {
                XT_TRY(xtAppendList(memoryDescs, new_node));
            }
            
            last_entry = mapEntry;
        }
        
        // Переходим к следующему дескриптору
        desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)desc + gKernelBootInfo->DescSize);
    }
    
    return XT_SUCCESS;
}

// Ваша оригинальная функция memcpy
XTResult xtMemSet(void* data, uint8_t c, uint64_t count) {
    XT_CHECK_ARG_IS_NULL(data);
    
    for(uint64_t i = 0; i < count; ++i) {
        ((uint8_t*)data)[i] = c;
    }
    return XT_SUCCESS;
}