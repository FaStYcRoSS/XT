#include <xt/result.h>
#include <xt/memory.h>

XTResult xtStringCmp(const char* left, const char* right, uint64_t max) {

    for (uint64_t i = 0;*left && *right && i < max;++left,++right, ++i) {
        if ((*left - *right) != 0) return XT_NOT_EQUAL;
    }
    return XT_SUCCESS;

}

XTResult xtCopyMem(void* dst, void* src, uint64_t size) {
    for(uint64_t i = 0; i < size; ++i) {
        ((char*)dst)[i] = ((char*)src)[i];
    }
    return XT_SUCCESS;
}

XTResult xtSetMem(void* dst, uint8_t c, uint64_t size) {
    for(uint64_t i = 0; i < size; ++i) {
        ((char*)dst)[i] = c;
    }
    return XT_SUCCESS;
}