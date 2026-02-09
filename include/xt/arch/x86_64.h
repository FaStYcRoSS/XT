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

#endif