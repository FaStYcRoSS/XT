#include <xt/memory.h>
#include <xt/kernel.h>

#include <stdint.h>

typedef struct PageTable {
    uint64_t entries[512];
} PageTable;

#define GET_PAGE_TABLE(page_table, pos) (PageTable*)((page_table)->entries[pos] & (~(0xfff | (1ull << 63))))
#define SET_PAGE_TABLE(page_table, pos, entry, flags) ((page_table)->entries[pos] = (uint64_t)(entry) | flags)

#define IS_ALIGN(x, alignment) ((x & ~(alignment)) == 0)

#define P_PRESENT   (1ull<<0)
#define P_RW        (1ull<<1)
#define P_US        (1ull<<2)
#define P_PWT       (1ull<<3)
#define P_PCD       (1ull<<4)
#define P_ACCESSED  (1ull<<5)
#define P_DIRTY     (1ull<<6)
#define P_PS        (1ull<<7)   // large page (PDPT=1GiB, PD=2MiB)
#define P_GLOBAL    (1ull<<8)
#define P_NX        (1ull<<63)

#define ADDR_MASK   0x000ffffffffff000ull

#define PTE_FLAGS_MASK (0x7FFULL | ~(P_NX))
#define PTE_ADDR(Addr) ((UINT64)(Addr) & ~(PTE_FLAGS_MASK))

#define TABLE_MAX_ENTRIES 512ULL

#define PAGE_4KB 0x1000
#define PAGE_2MB 0x200000
#define PAGE_1GB 0x40000000

XTResult xtGetPageTable(void* pageTable, uint64_t index, void** out) {

    XT_CHECK_ARG_IS_NULL(pageTable);
    XT_CHECK_ARG_IS_NULL(out);

    PageTable* _pageTable = (PageTable*)pageTable;
    uint64_t e = _pageTable->entries[index];
    *out = HIGHER_HALF_MEM(e & ADDR_MASK);


    return XT_SUCCESS;

}

XTResult xtGetOrMakePageTable(void* pageTable, uint64_t index, void** out, uint64_t flags) {
    XT_CHECK_ARG_IS_NULL(pageTable);
    XT_CHECK_ARG_IS_NULL(out);

    PageTable* _pageTable = HIGHER_HALF_MEM((PageTable*)pageTable);
    uint64_t e = _pageTable->entries[index];
    if (!(e & P_PRESENT) || (e & P_PS)) {
        void* NewPage = NULL;
        XT_TRY(xtAllocatePages(NULL, 0x1000, &NewPage));
        //xtDebugPrint("NewPage 0x%llx\n", NewPage);
        _pageTable->entries[index] = ((uint64_t)NewPage) | (e & PTE_FLAGS_MASK) | flags;
        *out = HIGHER_HALF_MEM(NewPage);
        return XT_SUCCESS;
    }
    *out = HIGHER_HALF_MEM((e & ADDR_MASK));
    return XT_SUCCESS;
    
}

XTResult xtGetPhysicalAddress(void* pageTable, void* virtualAddress, void** out) {
    XT_CHECK_ARG_IS_NULL(pageTable);
    XT_CHECK_ARG_IS_NULL(out);


    PageTable* _pageTable = HIGHER_HALF_MEM((PageTable*)pageTable);
    
    uint64_t va64 = virtualAddress;
    uint64_t PML4I = (va64 >> 39) & 0x1ff;
    uint64_t PDPI = (va64 >> 30) & 0x1ff;
    uint64_t PDI = (va64 >> 21) & 0x1ff;
    uint64_t PTI = (va64 >> 12) & 0x1ff;
    uint64_t pdpte = _pageTable->entries[PML4I];
    if (!(pdpte & P_PRESENT)) return XT_NOT_FOUND;
    PageTable* pdpt = (PageTable*)(HIGHER_HALF_MEM(pdpte & ADDR_MASK));
    uint64_t pdpe = pdpt->entries[PDPI];
    if (!(pdpe & P_PRESENT)) return XT_NOT_FOUND;
    if (pdpe & P_PS) {
        *out = (void*)((pdpe & ADDR_MASK) | (va64 & ((1ull << 30) - 1)));
        return XT_SUCCESS;
    }
    PageTable* pdp = (PageTable*)(HIGHER_HALF_MEM(pdpe & ADDR_MASK));
    uint64_t pde = pdp->entries[PDI];
    if (!(pde & P_PRESENT)) return XT_NOT_FOUND;
    if (pde & P_PS) {
        *out = (void*)((pde & ADDR_MASK) | (va64 & ((1ull << 21) - 1)));
        return XT_SUCCESS;
    }
    PageTable* pd = (PageTable*)(HIGHER_HALF_MEM(pde & ADDR_MASK));
    uint64_t pte = pd->entries[PTI];
    if (!(pte & P_PRESENT)) return XT_NOT_FOUND;
    *out = (void*)((pte & ADDR_MASK) | (va64 & ((1ull << 12) - 1)));
    return XT_SUCCESS;
    

}

uint64_t xtBuildFlags(uint64_t attr) {
    uint64_t flags = 1;
    if (attr & XT_MEM_USER) flags |= P_US;
    if (attr & XT_MEM_WRITE) flags |= P_RW;
    if (!(attr & XT_MEM_EXEC)) flags |= P_NX;
    return flags;
}



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

    uint64_t directory_flags = 0x07;
    uint64_t flags = xtBuildFlags(attributes);

    while (size) {
        uint64_t PML4I = (va64 >> 39) & 0x1ff;
        uint64_t PDPI = (va64 >> 30) & 0x1ff;
        uint64_t PDI = (va64 >> 21) & 0x1ff;
        uint64_t PTI = (va64 >> 12) & 0x1ff;

        uint64_t page_size = 0;

        if ( (va64 % (1ull<<30))==0 && (pa64 % (1ull<<30))==0 && size >= (1ull<<30) ) {
            page_size = 1ull<<30; // 1 GiB
        } else if ( (va64 % (1ull<<21))==0 && (pa64 % (1ull<<21))==0 && size >= (1ull<<21) ) {
            page_size = 1ull<<21; // 2 MiB
        } else {
            page_size = 1ull<<12; // 4 KiB
        }
        PageTable* pdpt = NULL;
        XT_TRY(xtGetOrMakePageTable(pml4, PML4I, &pdpt, directory_flags));
        //xtDebugPrint("va 0x%llx pdpt 0x%llx\n",va64, pdpt);
        if (page_size == (1ull << 30)) {
            uint64_t _pa64 = 0;
            if (pa64 == (uint64_t)(-1)) {
                _pa64 = pdpt->entries[PDPI];
            }
            else {
                _pa64 = pa64;
            }
            pdpt->entries[PDPI] = (_pa64 & ADDR_MASK) | flags | P_PS;
        }
        else {

            PageTable* pd = NULL;
            XT_TRY(xtGetOrMakePageTable(pdpt, PDPI, &pd, directory_flags));
            //xtDebugPrint("va 0x%llx pd 0x%llx\n",va64, pd);
            uint64_t _pa64 = 0;
            if (pa64 == (uint64_t)(-1)) {
                _pa64 = pd->entries[PDI];
            }
            else {
                _pa64 = pa64;
            }
            if (page_size == (1 << 21)) {
                pd->entries[PDI] = (_pa64 & ADDR_MASK) | flags | P_PS;
            }
            else {
                PageTable* pt = NULL;
                XT_TRY(xtGetOrMakePageTable(pd, PDI, &pt, directory_flags));
                //xtDebugPrint("va 0x%llx pt 0x%llx\n",va64, pt);
                uint64_t _pa64 = 0;
                if (pa64 == (uint64_t)(-1)) {
                    _pa64 = pt->entries[PTI];
                }
                else {
                    _pa64 = pa64;
                }
                pt->entries[PTI] = (_pa64 & ADDR_MASK) | flags;
            }
        }


        va64 += page_size;
        pa64 += page_size;
        size -= page_size;
    }

    return XT_SUCCESS;


}

// Вспомогательная функция: проверяет, есть ли в таблице хоть одна живая запись
static int xtIsTableEmpty(PageTable* table) {
    for (int i = 0; i < 512; i++) {
        // Если хотя бы одна страница Present, таблица не пуста
        if (table->entries[i] & P_PRESENT) {
            return 0;
        }
    }
    return 1;
}

XTResult xtUnsetPages(void* pageTable, void* virtual_address, uint64_t size) {
    XT_CHECK_ARG_IS_NULL(pageTable);

    PageTable* pml4 = (PageTable*)pageTable;
    uint64_t va64 = (uint64_t)virtual_address;

    while (size) {
        uint64_t PML4I = (va64 >> 39) & 0x1ff;
        uint64_t PDPI = (va64 >> 30) & 0x1ff;
        uint64_t PDI = (va64 >> 21) & 0x1ff;
        uint64_t PTI = (va64 >> 12) & 0x1ff;

        uint64_t page_size = PAGE_4KB;

        // --- Уровень 1: PML4 ---
        if (!(pml4->entries[PML4I] & P_PRESENT)) {
            page_size = PAGE_1GB * 512; // Пропускаем сразу 512 ГБ
            goto next_step;
        }
        PageTable* pdpt = (PageTable*)(pml4->entries[PML4I] & ADDR_MASK);

        // --- Уровень 2: PDPT ---
        if (!(pdpt->entries[PDPI] & P_PRESENT)) {
            page_size = PAGE_1GB;
            goto next_step;
        }

        if (pdpt->entries[PDPI] & P_PS) { // Огромная страница 1ГБ
            pdpt->entries[PDPI] = 0;
            page_size = PAGE_1GB;
        } else {
            PageTable* pd = (PageTable*)(pdpt->entries[PDPI] & ADDR_MASK);

            // --- Уровень 3: PD ---
            if (!(pd->entries[PDI] & P_PRESENT)) {
                page_size = PAGE_2MB;
                goto next_step;
            }

            if (pd->entries[PDI] & P_PS) { // Большая страница 2МБ
                pd->entries[PDI] = 0;
                page_size = PAGE_2MB;
            } else {
                PageTable* pt = (PageTable*)(pd->entries[PDI] & ADDR_MASK);

                // --- Уровень 4: PT ---
                pt->entries[PTI] = 0; // Зануляем PTE
                page_size = PAGE_4KB;

                // ПРОВЕРКА: Не стала ли PT пустой?
                if (xtIsTableEmpty(pt)) {
                    xtFreePages(pt, PAGE_4KB); // Освобождаем физическую страницу PT
                    pd->entries[PDI] = 0;      // Стираем ссылку на неё в PD
                }
            }

            // ПРОВЕРКА: Не стала ли PD пустой?
            if (xtIsTableEmpty(pd)) {
                xtFreePages(pd, PAGE_4KB);
                pdpt->entries[PDPI] = 0;
            }
        }

        // ПРОВЕРКА: Не стала ли PDPT пустой?
        if (xtIsTableEmpty(pdpt)) {
            xtFreePages(pdpt, PAGE_4KB);
            pml4->entries[PML4I] = 0;
        }

    next_step:
        // Сбрасываем кэш процессора для этого адреса
        __asm__ volatile("invlpg (%0)" ::"r"(va64) : "memory");

        va64 += page_size;
        if (size >= page_size) size -= page_size;
        else size = 0;
    }

    return XT_SUCCESS;
}