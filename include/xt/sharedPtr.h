#ifndef __XT_SHARED_PTR_H__
#define __XT_SHARED_PTR_H__

#include <xt/result.h>
#include <stdint.h>

typedef struct XTSharedPtr XTSharedPtr;

XTResult xtCreateSharedPtr(void* data, uint64_t size, XTSharedPtr** out);
XTResult xtSharedPtrGetData(XTSharedPtr* ptr, void** data);
XTResult xtIncrementReference(XTSharedPtr* ptr);
XTResult xtDecrementReference(XTSharedPtr* ptr);

#endif