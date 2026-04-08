#ifndef __XT_ENCODINGS_H__
#define __XT_ENCODINGS_H__

#include <xt/result.h>
#include <stdint.h>

XTResult xtUTF8toUTF16(const uint8_t* utf8, uint16_t* utf16);

XTResult xtUTF16toUTF8(const uint16_t* utf16, uint8_t* utf8);

#endif