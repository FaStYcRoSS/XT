
#include "../external/printf.h"

#include <xt/io.h>
#include <xt/result.h>

void _putchar(char _ch) {
    return;
}

void ___chkstk_ms() {
    
}

const char* results[] = {
    "SUCCESS",
    "INVALID_PARAMETER",
    "NOT_IMPLEMENTED",
    "OUT_OF_MEMORY",
    "OUT_OF_BOUNDARY",
    "NOT_FOUND",
    "NOT_EQUAL"
};

const char* xtResultToStr(XTResult result) {
    result &= ~XT_ERROR_MASK;
    return results[result];
}

int xtDebugPrint(const char* format, ...) {
    char buff[4096];

    va_list va;
    va_start(va, format);

    int n = vsnprintf(buff, 4096, format, va);

    uint64_t written = n;

    xtWriteFile(gSerialDevice, buff, &written);

    va_end(va);
    return n;
}
