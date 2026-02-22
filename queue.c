#include <xt/queue.h>
#include <xt/memory.h>

typedef struct XTQueueNode {
    void* data;
    struct XTQueueNode* next;
} XTQueueNode;  

struct XTQueue {
    XTQueueNode* head;
    XTQueueNode* tail;
};


XTResult xtCreateQueueNode(void* data, XTQueueNode** out) {
    XT_CHECK_ARG_IS_NULL(out);

    XTQueueNode* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTQueueNode), &result));
    result->data = data;
    result->next = NULL;
    *out = result;
    return XT_SUCCESS;

}

XTResult xtPopQueue(XTQueue* q, void** data) {
    XT_CHECK_ARG_IS_NULL(q);
    XT_CHECK_ARG_IS_NULL(q->head);
    XT_CHECK_ARG_IS_NULL(q->tail);
    void* _result = q->head->data;
    XTQueueNode* head = q->head;
    q->head = q->head->next;
    return xtHeapFree(head);
}

XTResult xtPushQueue(XTQueue* q, void* data) {
    XT_CHECK_ARG_IS_NULL(q);

    XTQueueNode* newNode = NULL;
    XT_TRY(xtCreateQueueNode(data, &newNode));
    if (q->tail == NULL) {
        q->tail = newNode;
    }
    else {
        q->tail->next = newNode;
        q->tail = newNode;
    }
    if (q->head == NULL) {
        q->head = q->tail;
    }
    return XT_SUCCESS;
}

XTResult xtCreateQueue(XTQueue** out) {
    XT_CHECK_ARG_IS_NULL(out);
    XTQueue* result = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTQueue), &result));
    result->head = NULL;
    result->tail = NULL;
    *out = result;
    return XT_SUCCESS;
}

XTResult xtDestroyQueue(XTQueue* result) {
    return xtHeapFree(result);
}

XTResult xtRemoveFromQueue(XTQueue* q, void* data) {
    for (XTQueueNode* i = q->head, *prev = NULL; i; prev = NULL, i = i->next) {
        if (i->data == data) {
            if (prev == NULL) {
                q->head = i->next;
            }
            else prev->next = i->next;
            xtHeapFree(i);
            return XT_SUCCESS;
        }   
    }
    return XT_NOT_FOUND;
}