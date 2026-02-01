#ifndef __XT_LIST_H__
#define __XT_LIST_H__

#include <xt/result.h>

#include <stdint.h>

typedef struct XTList XTList;

XTResult xtCreateList(void* data, XTList** out);

XTResult xtAppendList(XTList* l, XTList* to_append);

XTResult xtGetLastList(XTList* l, XTList** out);

XTResult xtGetNextList(XTList* l, XTList** out);

XTResult xtInsertList(XTList* l, XTList* next);

XTResult xtIndexList(XTList* l, uint64_t pos, XTList** out);

XTResult xtDestroyList(XTList* list);

#endif