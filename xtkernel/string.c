#include <xt/result.h>
#include <xt/memory.h>
#include <xt/string.h>

XTResult xtStringCmp(const char* left, const char* right, uint64_t max) {

    for (uint64_t i = 0;*left && *right && i < max;++left,++right, ++i) {
        if ((*left - *right) != 0) return XT_NOT_EQUAL;
    }
    if (*left || *right) return XT_NOT_EQUAL;
    return XT_SUCCESS;
}

int tolower(int _ch) {
    if (_ch >= 65 && _ch <= 90) return _ch + 32;
    return _ch;

}

XTResult xtStringICmp(const char* left, const char* right, uint64_t max) {
    for (uint64_t i = 0;*left && *right && i < max;++left,++right, ++i) {
        if ((tolower(*left) - tolower(*right)) != 0) return XT_NOT_EQUAL;
    }
    if (*left || *right) return XT_NOT_EQUAL;
    return XT_SUCCESS;
}

XTResult xtGetStringLength(const char* str, uint64_t* len) {
    XT_CHECK_ARG_IS_NULL(len);
    XT_CHECK_ARG_IS_NULL(str);
    uint64_t result = 0;
    for (;*str;++str, ++result);
    *len = result;
    return XT_SUCCESS;

}

XTResult xtCopyString(char* dst, const char* src, uint64_t max) {
    XT_CHECK_ARG_IS_NULL(dst);
    XT_CHECK_ARG_IS_NULL(src);
    XT_CHECK_ARG_IS_NULL(dst);
    XT_CHECK_ARG_IS_NULL(src);
    for(uint64_t i = 0; i < max && ((char*)src)[i]; ++i) {
        ((char*)dst)[i] = ((char*)src)[i];
    }
    return XT_SUCCESS;
}


XTResult xtCopyMem(void* dst, void* src, uint64_t size) {
    XT_CHECK_ARG_IS_NULL(dst);
    XT_CHECK_ARG_IS_NULL(src);
    for(uint64_t i = 0; i < size; ++i) {
        ((char*)dst)[i] = ((char*)src)[i];
    }
    return XT_SUCCESS;
}

XTResult xtDuplicateString(const char* str, const char** out) {
    XT_CHECK_ARG_IS_NULL(str);
    XT_CHECK_ARG_IS_NULL(out);
    char* newStr = NULL;
    uint64_t size = 0;
    xtGetStringLength(str, &size);
    XT_TRY(xtHeapAlloc(size+1, &newStr));
    xtCopyMem(newStr, str, size+1);
    *out = newStr;
    return XT_SUCCESS;
}


XTResult xtSetMem(void* dst, uint8_t c, uint64_t size) {
    XT_CHECK_ARG_IS_NULL(dst);
    for(uint64_t i = 0; i < size; ++i) {
        ((char*)dst)[i] = c;
    }
    return XT_SUCCESS;
}