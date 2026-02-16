#ifndef __XT_LIST_H__
#define __XT_LIST_H__

#include <xt/result.h>

#include <stdint.h>

typedef struct XTList XTList;

struct XTList {
    void* data;
    struct XTList* next;
};


XTResult xtCreateList(void* data, XTList** out);

XTResult xtAppendList(XTList* l, XTList* to_append);

XTResult xtGetLastList(XTList* l, XTList** out);

XTResult xtGetNextList(XTList* l, XTList** out);

XTResult xtInsertList(XTList* l, XTList* next);

XTResult xtIndexList(XTList* l, uint64_t pos, XTList** out);

XTResult xtDestroyList(XTList* list);

XTResult xtRemoveFromList(XTList* list, XTList* toRemove);

XTResult xtGetListData(XTList* list, void** data);
XTResult xtSetListData(XTList* list, void* data);

XTResult xtListLength(XTList* list, uint64_t* length);

XTResult xtSetNextList(XTList* l, XTList* next);

#endif