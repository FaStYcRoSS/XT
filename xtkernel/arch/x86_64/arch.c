#include <xt/arch/x86_64.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/time.h>

#define CURRENT_YEAR 2026

XTResult xtDTInit();
XTResult xtEarlyRandomInit();
XTResult xtArchACPIInit();

extern void* kernelPageTable;

static uint8_t getUpdateInProgress() {
    xtOutB(0x70, 0x0a);
    uint8_t data = 0;
    xtInB(0x71, &data);
    return data & 0x80;
}

static uint8_t getRTCRegister(int reg) {
    xtOutB(0x70, reg);
    uint8_t data = 0;
    xtInB(0x71, &data);
    return data;
}

XTResult xtCMOSInit() {
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned int year;
    unsigned char century;
    unsigned char last_second;
    unsigned char last_minute;
    unsigned char last_hour;
    unsigned char last_day;
    unsigned char last_month;
    unsigned char last_year;
    unsigned char last_century;
    unsigned char registerB;
    int century_register = 0x00;                                // Set by ACPI table parsing code if possible

    // Note: This uses the "read registers until you get the same values twice in a row" technique
    //       to avoid getting dodgy/inconsistent values due to RTC updates

    while (getUpdateInProgress());                // Make sure an update isn't in progress
    second = getRTCRegister(0x00);
    minute = getRTCRegister(0x02);
    hour = getRTCRegister(0x04);
    day = getRTCRegister(0x07);
    month = getRTCRegister(0x08);
    year = getRTCRegister(0x09);
    if(century_register != 0) {
        century = getRTCRegister(century_register);
    }

    do {
        last_second = second;
        last_minute = minute;
        last_hour = hour;
        last_day = day;
        last_month = month;
        last_year = year;
        last_century = century;

        while (getUpdateInProgress());           // Make sure an update isn't in progress
        second = getRTCRegister(0x00);
        minute = getRTCRegister(0x02);
        hour = getRTCRegister(0x04);
        day = getRTCRegister(0x07);
        month = getRTCRegister(0x08);
        year = getRTCRegister(0x09);
        if(century_register != 0) {
                century = getRTCRegister(century_register);
        }
    } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
            (last_day != day) || (last_month != month) || (last_year != year) ||
            (last_century != century) );

    registerB = getRTCRegister(0x0B);

    // Convert BCD to binary values if necessary

    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
        day = (day & 0x0F) + ((day / 16) * 10);
        month = (month & 0x0F) + ((month / 16) * 10);
        year = (year & 0x0F) + ((year / 16) * 10);
        if(century_register != 0) {
                century = (century & 0x0F) + ((century / 16) * 10);
        }
    }

    // Convert 12 hour clock to 24 hour clock if necessary

    if (!(registerB & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }

    // Calculate the full (4-digit) year

    if(century_register != 0) {
        year += century * 100;
    } else {
        year += (CURRENT_YEAR / 100) * 100;
        if(year < CURRENT_YEAR) year += 100;
    }
    XTTime time = {
        .year = year,
        .month = month,
        .mday = day,
        .hour = hour,
        .minutes = minute,
        .seconds = second
    };
    uint64_t unixtime = 0;
    xtMakeTime(&time, &unixtime);
    xtSetTime(unixtime, 0);
    return XT_SUCCESS;
}


XTResult xtArchInit() {
    asm volatile("mov %%cr3, %%rax":"=a"(kernelPageTable));
    XT_TRY(xtDTInit());
    XT_TRY(xtSyscallInit());
    XT_TRY(xtEarlyRandomInit());
    

    void* vdso = NULL;
    XT_ASSERT(xtFindSection(KERNEL_IMAGE_BASE, ".vdso", &vdso));
    XT_ASSERT(xtGetPhysicalAddress(kernelPageTable, vdso, &vdso));

    xtSetPages(kernelPageTable, 
        (void*)(0x00007ffffffff000),
        vdso,
        0x1000,
        XT_MEM_EXEC | XT_MEM_READ | XT_MEM_USER
    );
    xtCMOSInit();
    xtArchACPIInit();
    return XT_SUCCESS;
}