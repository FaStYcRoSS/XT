#include <xt/arch/x86_64.h>

#include <xt/memory.h>

typedef struct GDTEntry {
    uint16_t limit0;
    uint16_t base0;
    uint8_t  base1;
    uint8_t  access;
    uint8_t  flagsAndLimit;
    uint8_t  base2;
    uint64_t base3;
} GDTEntry;

typedef struct GateDesc {
    uint16_t offset0;
    uint16_t segmentSelector;
    uint16_t flags;
    uint16_t offset1;
    uint64_t offset2;
} GateDesc;

GateDesc* idt = NULL;

GDTEntry gdt[5] = {
    {
        0
    },
    {
        .limit0 = 0xffff,
        .base0  = 0x0,
        .base1  = 0x0,
        .base2  = 0x0,
        .base3  = 0x0,
        .access = 0x9a,
        .flagsAndLimit = 0xc
    },
    {
        .limit0 = 0xffff,
        .base0  = 0x0,
        .base1  = 0x0,
        .base2  = 0x0,
        .base3  = 0x0,
        .access = 0x92,
        .flagsAndLimit = 0xc
    },
    {
        .limit0 = 0xffff,
        .base0  = 0x0,
        .base1  = 0x0,
        .base2  = 0x0,
        .base3  = 0x0,
        .access = 0xfa,
        .flagsAndLimit = 0xa
    },
    {
        .limit0 = 0xffff,
        .base0  = 0x0,
        .base1  = 0x0,
        .base2  = 0x0,
        .base3  = 0x0,
        .access = 0xf2,
        .flagsAndLimit = 0xc
    },
};

XTResult xtDTInit() {


    XT_TRY(xtAllocatePages(NULL, 0x1000, &idt));
    return XT_SUCCESS;

}
