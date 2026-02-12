#include <xt/random.h>
static uint64_t boot_seed;

XTResult xtEarlyRandomInit() {
    uint32_t low, high;
    // Для x86_64
    __asm__ __volatile__ ("rdtsc" : "=a"(low), "=d"(high));
    boot_seed = ((uint64_t)high << 32) | low;
    
    // Если процессор современный (Ivy Bridge и новее), 
    // можно попробовать инструкцию RDRAND — это честный аппаратный рандом.
    // Но rdtsc хватит для начала.
}

XTResult xtGetRandomU64(uint64_t* random) {
    XT_CHECK_ARG_IS_NULL(random);
    uint64_t x = boot_seed;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    boot_seed = x;
    *random = x;
    return XT_SUCCESS;
}