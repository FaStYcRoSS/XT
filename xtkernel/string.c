#include <xt/result.h>
#include <xt/memory.h>

XTResult xtStringCmp(const char* left, const char* right, uint64_t max) {

    for (uint64_t i = 0;*left && *right && i < max;++left,++right, ++i) {
        if ((*left - *right) != 0) return XT_NOT_EQUAL;
    }
    return XT_SUCCESS;

}