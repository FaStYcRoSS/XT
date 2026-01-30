#include <xt/memory.h>
#include <xt/kernel.h>
#include <xt/efi.h>

static uint8_t heap_data[256 * 1024];

typedef struct XTHeapEntry {
    uint64_t prev;
    int64_t next;
    uint8_t data[1];
} XTHeapEntry;

int64_t abs(int64_t x) {
    return x < 0 ? -x : x;
}

#define XT_NEXT_HEAP(heap_entry) ((XTHeapEntry*)((char*)heap_entry + abs((heap_entry)->next)))
#define XT_PREV_HEAP(heap_entry) ((XTHeapEntry*)((char*)heap_entry - (heap_entry)->prev))

XTHeapEntry* xtGetHeapEntryNext(XTHeapEntry* entry) {

    if (abs(entry->next) == 0) return NULL;

    return XT_NEXT_HEAP(entry);
}


XTHeapEntry* xtGetHeapEntryPrev(XTHeapEntry* entry) {

    if (entry->prev == 0) return NULL;

    return XT_PREV_HEAP(entry);

}

XTResult xtHeapAlloc(uint64_t size, void** out) {
    // 1. Правильное выравнивание
    size = (size + 0xF) & ~0xFull; 

    XTHeapEntry* heap_entry = (XTHeapEntry*)heap_data;

    while (heap_entry) {
        if (heap_entry->next < 0) { // Блок занят
            heap_entry = xtGetHeapEntryNext(heap_entry);
            continue;
        }

        uint64_t currentBlockSize = (uint64_t)heap_entry->next;

        if (currentBlockSize >= size) {
            // Можно ли отрезать кусок? 
            // Нужно место хотя бы под заголовок (16 байт) + 16 байт данных
            if (currentBlockSize >= size + 0x10 + 16) {
                uint64_t remaining = currentBlockSize - size - 0x10;
                
                heap_entry->next = -(int64_t)size; // Помечаем как занятый
                
                XTHeapEntry* next_block = (XTHeapEntry*)((char*)heap_entry + size);
                next_block->next = (int64_t)remaining;
                next_block->prev = size + 0x10;
                
                // Обновляем prev следующего за ним блока, если он есть
                XTHeapEntry* after_next = xtGetHeapEntryNext(next_block);
                if (after_next) {
                    after_next->prev = remaining + 0x10;
                }
            } else {
                // Занимаем блок целиком
                heap_entry->next = -(int64_t)currentBlockSize;
            }
            
            *out = heap_entry->data;
            return XT_SUCCESS;
        }
        heap_entry = xtGetHeapEntryNext(heap_entry);
    }
    return XT_OUT_OF_MEMORY;
}

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

XTResult xtMemoryInit(KernelBootInfo* bootInfo) {
    XTHeapEntry* entry = (XTHeapEntry*)heap_data;
    entry->next = 256*1024-0x10;
    entry->prev = 0;

    kprintf("DescSize 0x%x\nMemoryMapSize 0x%x\n", bootInfo->DescSize, bootInfo->MemoryMapSize);

    for (EFI_MEMORY_DESCRIPTOR* desc = &bootInfo->descs[0]; 
        desc < (char*)&bootInfo->descs[0]+bootInfo->MemoryMapSize; 
        desc = (char*)desc+bootInfo->DescSize) {
            kprintf("0x%llx-0x%llx %s 0x%llx\n", 
                desc->PhysicalStart, 
                desc->PhysicalStart+desc->NumberOfPages*4096, 
                MemoryTypeStr[desc->Type],
                desc->Attribute
            );
        }

    return XT_SUCCESS;
}

XTResult xtHeapFree(void* ptr) {

    uint64_t addr_ptr = ptr;
    addr_ptr -= 0x10;
    kprintf("addr_ptr 0x%llx\n", addr_ptr);
    XTHeapEntry* entry = (XTHeapEntry*)addr_ptr;
    entry->next = -entry->next;
    XTHeapEntry* next = xtGetHeapEntryNext(entry);
    if (next != NULL) {
        if (next->next >= 0) {
            entry->next += next->next+0x10;
            XTHeapEntry* last = xtGetHeapEntryNext(entry);
            if (last != NULL)
                last->prev += entry->next + 0x10;
        }
    }
    XTHeapEntry* past = xtGetHeapEntryPrev(entry);
    if (past != NULL) {
        if (past->next >= 0) {
            past->next += entry->next+0x10;
        }
    }
    return XT_SUCCESS;
}

