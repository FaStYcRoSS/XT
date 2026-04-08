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
    // Проверку q->tail можно убрать, так как если head не NULL, то и tail должен быть.
    
    void* _result = q->head->data;
    XTQueueNode* head = q->head;
    
    q->head = q->head->next;
    
    // Чиним хвост очереди, если она опустела
    if (q->head == NULL) {
        q->tail = NULL; 
    }
    
    // Возвращаем данные наружу
    if (data != NULL) {
        *data = _result;
    }
    
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
    for (XTQueueNode* i = q->head, *prev = NULL; i != NULL; prev = i, i = i->next) { // <-- Исправлен шаг
        if (i->data == data) {
            if (prev == NULL) {
                q->head = i->next;
            }
            else {
                prev->next = i->next;
            }
            
            // Если удалили хвост, нужно обновить q->tail
            if (i->next == NULL) {
                q->tail = prev;
            }
            
            xtHeapFree(i);
            return XT_SUCCESS;
        }   
    }
    return XT_NOT_FOUND;
}