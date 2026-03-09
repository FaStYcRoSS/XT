#include <xt/user.h>
#include <stdint.h>

#include "../../external/printf.h"

void ___chkstk_ms() {

}

void _putchar(char _ch) {

}

XTResult xtMain(const char** args) {
    char buff[4096];
    uint64_t gs = 0;
    asm volatile("lea %%gs:0, %%rax":"=a"(gs));
    int n = snprintf(buff, 4096, "args[0] %s gs=0x%llx\n", args[0], gs);
    char* null = NULL;
    *null = 0;
    return xtUserWriteFile(0, buff, n, (void*)0);
}
