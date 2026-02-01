#include <xt/memory.h>

#include <xt/list.h>

struct XTList {
    void* data;
    struct XTList* next;
};

XTResult xtCreateList(void* data, XTList** out) {
    XT_CHECK_NULL(out);
    XTList* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTList), &result));
    result->data = data;
    result->next = NULL;
    *out = result;
    return XT_SUCCESS;
}

XTResult xtAppendList(XTList* l, XTList* to_append) {
    XT_CHECK_NULL(l);
    XT_CHECK_NULL(to_append);

    XTList* last = NULL;
    XT_TRY(xtGetLastList(l, &last));
    last->next = to_append;
    return XT_SUCCESS;

}

XTResult xtGetLastList(XTList* l, XTList** out) {
    XT_CHECK_NULL(l);
    XT_CHECK_NULL(out);

    for (;l->next; l = l->next);
    *out = l;
    return XT_SUCCESS;

}

XTResult xtGetNextList(XTList* l, XTList** out) {

    XT_CHECK_NULL(l);
    XT_CHECK_NULL(out);

    *out = l->next;
    return XT_SUCCESS;
}

XTResult xtInsertList(XTList* l, XTList* next) {
    XT_CHECK_NULL(l);
    XT_CHECK_NULL(next);
    XTList* last = NULL;
    xtGetLastList(next, &last);
    last->next = l->next;
    l->next = next;
    return XT_SUCCESS;
}

XTResult xtIndexList(XTList* l, uint64_t pos, XTList** out) {

    XT_CHECK_NULL(l);
    XT_CHECK_NULL(out);

    for (uint64_t i = 0; i < pos && l; ++i, l = l->next);
    if (l == NULL) return XT_OUT_OF_BOUNDARY;
    *out = l;
    return XT_SUCCESS;
}

XTResult xtDestroyList(XTList* list) {
    XT_CHECK_NULL(list);
    XT_TRY(xtHeapFree(list));
}