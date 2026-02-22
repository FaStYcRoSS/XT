#include <xt/result.h>
#include <xt/memory.h>
#include <xt/string.h>

// Вспомогательная функция (лучше использовать символы, а не магические числа)
int tolower(int _ch) {
    if (_ch >= 'A' && _ch <= 'Z') return _ch + ('a' - 'A');
    return _ch;
}

// Безопасное сравнение памяти
XTResult xtCompareMemory(const void* left, const void* right, uint64_t len) {
    const uint8_t* l = (const uint8_t*)left;
    const uint8_t* r = (const uint8_t*)right;

    for (uint64_t i = 0; i < len; ++i) {
        if (l[i] != r[i]) return XT_NOT_EQUAL;
    }
    return XT_SUCCESS;
}

// Сравнение строк (аналог strncmp, но возвращает XTResult)
XTResult xtStringCmp(const char* left, const char* right, uint64_t max) {
    if (max == 0) return XT_SUCCESS;

    for (uint64_t i = 0; i < max; ++i) {
        if (left[i] != right[i]) return XT_NOT_EQUAL;
        if (left[i] == '\0') return XT_SUCCESS; // Строки закончились одновременно
    }
    return XT_SUCCESS;
}

// Регистронезависимое сравнение строк
XTResult xtStringICmp(const char* left, const char* right, uint64_t max) {
    if (max == 0) return XT_SUCCESS;

    for (uint64_t i = 0; i < max; ++i) {
        if (tolower((unsigned char)left[i]) != tolower((unsigned char)right[i])) 
            return XT_NOT_EQUAL;
        if (left[i] == '\0') return XT_SUCCESS;
    }
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