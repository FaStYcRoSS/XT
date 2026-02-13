#ifndef __XT_STRING_H__
#define __XT_STRING_H__

#include <xt/result.h>
#include <stdint.h>

XTResult xtStringCmp(const char* left, const char* right, uint64_t max);
XTResult xtCopyMem(void* dst, void* src, uint64_t size);
XTResult xtSetMem(void* dst, uint8_t c, uint64_t size);
XTResult xtStringICmp(const char* left, const char* right, uint64_t max);
XTResult xtDuplicateString(const char* str, const char** out);
XTResult xtGetStringLength(const char* str, uint64_t* len);
XTResult xtCopyString(char* dst, const char* src, uint64_t max);

#endif