#include <xt/sharedPtr.h>
#include <xt/memory.h>

struct XTSharedPtr {
    void* data;
    uint64_t count;
};

XTResult xtSharedPtrGetData(XTSharedPtr* ptr, void** data) {
    XT_CHECK_ARG_IS_NULL(ptr);
    XT_CHECK_ARG_IS_NULL(data);
    *data = ptr->data;
    return XT_SUCCESS;
}

XTResult xtCreateSharedPtr(void* data, uint64_t size, XTSharedPtr** out) {
    XT_CHECK_ARG_IS_NULL(out);

    XT_TRY(xtHeapAlloc(size, &data));

    XTResult result = 0;
    XTSharedPtr* sharedPtr = NULL;
    result = xtHeapAlloc(sizeof(XTSharedPtr), &sharedPtr);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(data);
        return result;
    }
    sharedPtr->count = 0;
    sharedPtr->data = data;
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
        XT_TRY(xtHeapFree(ptr->data));
    }
    return XT_SUCCESS;
}