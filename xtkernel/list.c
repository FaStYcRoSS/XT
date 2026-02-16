#include <xt/memory.h>

#include <xt/list.h>

XTResult xtCreateList(void* data, XTList** out) {
    XT_CHECK_ARG_IS_NULL(out);
    XTList* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTList), &result));
    result->data = data;
    result->next = NULL;
    *out = result;
    return XT_SUCCESS;
}

XTResult xtSetListData(XTList* list, void* data) {
    XT_CHECK_ARG_IS_NULL(list);
    XT_CHECK_ARG_IS_NULL(data);
    list->data = data;
    return XT_SUCCESS;
}

XTResult xtGetListData(XTList* list, void** data) {

    XT_CHECK_ARG_IS_NULL(list);
    XT_CHECK_ARG_IS_NULL(data);

    *data = list->data;
    return XT_SUCCESS;
}

XTResult xtAppendList(XTList* l, XTList* to_append) {
    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(to_append);

    XTList* last = NULL;
    XT_TRY(xtGetLastList(l, &last));
    last->next = to_append;
    return XT_SUCCESS;

}

XTResult xtSetNextList(XTList* l, XTList* next) {
    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(next);
    l->next = next;
    return XT_SUCCESS;
}

XTResult xtListLength(XTList* l, uint64_t* length) {
    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(length);
    uint64_t len = 0;
    for (;l;l=l->next,++len);
    *length = len;
}

XTResult xtGetLastList(XTList* l, XTList** out) {
    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(out);

    for (;l->next; l = l->next);
    *out = l;
    return XT_SUCCESS;

}

XTResult xtGetNextList(XTList* l, XTList** out) {

    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(out);

    *out = l->next;
    return XT_SUCCESS;
}

XTResult xtInsertList(XTList* l, XTList* next) {
    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(next);
    XTList* last = NULL;
    xtGetLastList(next, &last);
    last->next = l->next;
    l->next = next;
    return XT_SUCCESS;
}

XTResult xtIndexList(XTList* l, uint64_t pos, XTList** out) {

    XT_CHECK_ARG_IS_NULL(l);
    XT_CHECK_ARG_IS_NULL(out);

    for (uint64_t i = 0; i < pos && l; ++i, l = l->next);
    if (l == NULL) return XT_OUT_OF_BOUNDARY;
    *out = l;
    return XT_SUCCESS;
}

XTResult xtRemoveFromList(XTList* list, XTList* toRemove) {
    XT_CHECK_ARG_IS_NULL(list);
    XT_CHECK_ARG_IS_NULL(toRemove);

    for (;list->next && list->next == toRemove; list = list->next);
    if (list->next == NULL) return XT_NOT_FOUND;

    list->next = list->next->next;
    xtDestroyList(toRemove);

    return XT_SUCCESS;

}

XTResult xtDestroyList(XTList* list) {
    XT_CHECK_ARG_IS_NULL(list);
    XT_TRY(xtHeapFree(list));
}