#include <xt/arch/x86_64.h>

XTResult xtOutB(uint16_t port, uint8_t data) {
    asm volatile("outb %%al, %%dx"::"a"(data),"d"(port));
    return XT_SUCCESS;
}
XTResult xtOutW(uint16_t port, uint16_t data) {
    asm volatile("outw %%ax, %%dx"::"a"(data),"d"(port));
    return XT_SUCCESS;
}
XTResult xtOutL(uint16_t port, uint32_t data) {
    asm volatile("outl %%eax, %%dx"::"a"(data),"d"(port));
    return XT_SUCCESS;
}

XTResult xtInB(uint16_t port, uint8_t* data) {
    asm volatile("inb %%dx, %%al":"=a"(*data):"d"(port));
    return XT_SUCCESS;
}
XTResult xtInW(uint16_t port, uint16_t* data) {
    asm volatile("inw %%dx, %%ax":"=a"(*data):"d"(port));
    return XT_SUCCESS;
}
XTResult xtInL(uint16_t port, uint32_t* data) {
    asm volatile("inl %%dx, %%eax":"=a"(*data):"d"(port));
    return XT_SUCCESS;
}