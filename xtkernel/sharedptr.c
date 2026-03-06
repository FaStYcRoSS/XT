#include <xt/sharedPtr.h>
#include <xt/memory.h>
#include <xt/kernel.h>

struct XTSharedPtr {
    void* data;
    PFNXTDELETEFUNC deleteFunc;
    uint64_t count;
};

XTResult xtSharedPtrGetData(XTSharedPtr* ptr, void** data) {
    XT_CHECK_ARG_IS_NULL(ptr);
    XT_CHECK_ARG_IS_NULL(data);
    *data = ptr->data;
    return XT_SUCCESS;
}

XTResult xtCreateSharedPtr(void* data, XTSharedPtr** out, PFNXTDELETEFUNC deleteFunc) {
    XT_CHECK_ARG_IS_NULL(out);

    XTResult result = 0;
    XTSharedPtr* sharedPtr = NULL;
    result = xtHeapAlloc(sizeof(XTSharedPtr), &sharedPtr);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(data);
        return result;
    }
    sharedPtr->count = 1;
    sharedPtr->data = data;
    sharedPtr->deleteFunc = deleteFunc;
    *out = sharedPtr;
    return XT_SUCCESS;

}

XTResult xtIncrementReference(XTSharedPtr* ptr) {
    XT_CHECK_ARG_IS_NULL(ptr);
    ++ptr->count;
    return XT_SUCCESS;
}

XTResult xtDecrementReference(XTSharedPtr* ptr) {
    XT_CHECK_ARG_IS_NULL(ptr);
    --ptr->count;
    if (ptr->count == 0) {
        if (ptr->deleteFunc) {
            XTResult result = ptr->deleteFunc(ptr->data);
        }
        XT_TRY(xtHeapFree(ptr->data));
    }
    return XT_SUCCESS;
}