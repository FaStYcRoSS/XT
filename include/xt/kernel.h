#ifndef __XT_KERNEL_H__
#define __XT_KERNEL_H__

#include <xt/efi.h>

#include <xt/result.h>

int xtDebugPrint(const char* format, ...);

const char* xtResultToStr(XTResult result);

XTResult xtKernelPanic(const char* description, void* instruction, XTResult result);

#define XT_ASSERT(x) do { \
    XTResult __result = x; \
    if (XT_IS_ERROR(__result)) {\
        xtDebugPrint("%d:%s:%s\n", __LINE__, __FILE__, __FUNCTION__, #x, xtResultToStr(__result));\
        xtKernelPanic("ASSERT:", NULL, __result);\
    }\
} while(0)

extern KernelBootInfo* gKernelBootInfo;

#define HIGH_MEM (0xffff800000000000)
#define HIGHER_MEM(x) (((uint64_t) (x)) | HIGH_MEM)

#define XTEXPORT __declspec(dllexport)


#endif