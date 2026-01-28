
#include "../external/printf.h"

int kprintf(const char* format, ...) {
    char buff[4096];

    va_list va;
    va_start(va, format);

    vsnprintf(buff, 4096, format, va);



    va_end(va);

}
