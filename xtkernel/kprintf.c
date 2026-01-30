
#include "../external/printf.h"

void outb(unsigned short port, unsigned char _ch) {
    asm volatile("outb %%al, %%dx"::"d"(port),"a"(_ch));
}

static int kputs(const char* str) {
    for (;*str;++str) {
        outb(0x3f8, *str);
    }
    return 0;
}

void _putchar(char _ch) {
    return;
}

void ___chkstk_ms() {
    
}

int kprintf(const char* format, ...) {
    char buff[4096];

    va_list va;
    va_start(va, format);

    int n = vsnprintf(buff, 4096, format, va);

    kputs(buff);

    va_end(va);
    return n;
}
