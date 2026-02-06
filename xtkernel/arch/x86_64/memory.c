#include <xt/memory.h>
#include <xt/kernel.h>

#include <stdint.h>

typedef struct PageTable {
    uint64_t entries[512];
} PageTable;

#define GET_PAGE_TABLE(page_table, pos) (PageTable*)((page_table)->entries[pos] & (~(0xfff | (1ull << 63))))
#define SET_PAGE_TABLE(page_table, pos, entry, flags) ((page_table)->entries[pos] = (uint64_t)(entry) | flags)

#define IS_ALIGN(x, alignment) ((x & ~(alignment)) == 0)

XTResult xtSetPages(
    void* pageTable, 
    void* virtual_address,
    void* physical_address,
    uint64_t size, 
    uint64_t attributes
) {

    XT_CHECK_ARG_IS_NULL(pageTable);

    PageTable* pml4 = (PageTable*)pageTable;

    uint64_t va64 = (uint64_t)virtual_address;
    uint64_t pa64 = (uint64_t)physical_address;

    while (size) {
        uint64_t PML4I = (va64 >> 39) & 0x1ff;
        uint64_t PDPI = (va64 >> 30) & 0x1ff;
        uint64_t PDI = (va64 >> 21) & 0x1ff;
        uint64_t PTI = (va64 >> 12) & 0x1ff;

        uint64_t pdpe = pml4->entries[PML4I];
        PageTable* pdp = NULL;
        if (!(pdpe & 0x1)) {
            xtAllocatePages(NULL, 0x1000, &pdp);
        }

    }




}

XTResult xtUnsetPages(void* pageTable, void* virtual_address, uint64_t size) {

}
