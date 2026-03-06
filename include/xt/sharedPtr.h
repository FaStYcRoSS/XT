#ifndef __XT_SHARED_PTR_H__
#define __XT_SHARED_PTR_H__

#include <xt/result.h>
#include <stdint.h>

typedef struct XTSharedPtr XTSharedPtr;

typedef XTResult(*PFNXTDELETEFUNC)(void* data);

XTResult xtCreateSharedPtr(void* data, XTSharedPtr** out, PFNXTDELETEFUNC deleteFunc);
XTResult xtSharedPtrGetData(XTSharedPtr* ptr, void** data);
XTResult xtIncrementReference(XTSharedPtr* ptr);
XTResult xtDecrementReference(XTSharedPtr* ptr);

#endif