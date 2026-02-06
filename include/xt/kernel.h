#ifndef __XT_KERNEL_H__
#define __XT_KERNEL_H__

#include <xt/efi.h>

#include <xt/result.h>

int xtDebugPrint(const char* format, ...);

const char* xtResultToStr(XTResult result);

#define XT_ASSERT(x) do { \
    XTResult __result = x; \
    if (XT_IS_ERROR(__result)) {\
        xtDebugPrint("%d:%s:%s %s %s\n", __LINE__, __FILE__, __FUNCTION__, #x, xtResultToStr(__result));\
        while(1);\
    }\
} while(0)

extern KernelBootInfo* gKernelBootInfo;

#endif