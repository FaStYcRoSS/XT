#include <xt/kernel.h>
#include <xt/linker.h>

extern void xtHalt();

__attribute__((noinline)) __attribute__((noreturn))
XTResult XTEXPORT xtKernelPanic(const char* description, void* instruction, XTResult result) {
    if (instruction == NULL) {
        instruction = __builtin_return_address(0);
    }
    const char* strResult = xtResultToStr(result);
    const char* filenameOfImage = NULL;
    result = xtFindModuleByPtr(instruction, &filenameOfImage);
    if (result == XT_NOT_FOUND) {
        filenameOfImage = "(unknown)"; 
    }
    xtDebugPrint("%s:0x%llx - %s: %s\n", filenameOfImage, instruction, strResult, description);
    while (1) xtHalt();
}