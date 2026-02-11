#ifndef __XT_ARCH_X86_64_H__
#define __XT_ARCH_X86_64_H__

#include <xt/result.h>

#include <stdint.h>

#define PAGE_SHIFT 12
#define USER_SIZE_MAX ((1ull << 47) - 4096)

XTResult xtOutB(uint16_t port, uint8_t data);
XTResult xtOutW(uint16_t port, uint16_t data);
XTResult xtOutL(uint16_t port, uint32_t data);

XTResult xtInB(uint16_t port, uint8_t* data);
XTResult xtInW(uint16_t port, uint16_t* data);
XTResult xtInL(uint16_t port, uint32_t* data);

#include <stdint.h>

typedef struct XTContext {
    // Состояние, которое мы пушим вручную
    uint64_t rax, rbx, rcx, rdx, rbp, rsi, rdi, r8, 
                r9, r10, r11, r12, r13, r14, r15;
    uint64_t interruptNumber;
    uint64_t errorCode;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp; // Указатель стека (пользовательский!)
    uint64_t ss;
} XTContext;
#endif