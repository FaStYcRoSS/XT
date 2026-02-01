#ifndef __XT_ARCH_X86_64_H__
#define __XT_ARCH_X86_64_H__

#include <xt/result.h>

#include <stdint.h>

XTResult xtOutB(uint16_t port, uint8_t data);
XTResult xtOutW(uint16_t port, uint16_t data);
XTResult xtOutL(uint16_t port, uint32_t data);

XTResult xtInB(uint16_t port, uint8_t* data);
XTResult xtInW(uint16_t port, uint16_t* data);
XTResult xtInL(uint16_t port, uint32_t* data);

#endif