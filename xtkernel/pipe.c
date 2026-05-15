#include <xt/io.h>
#include <xt/kernel.h>
#include <xt/string.h>
#include <xt/memory.h>
#include <xt/event.h> // Используем твой новый API событий
#include <stdbool.h>

typedef struct XTPipeData {
    void* buffer;
    uint64_t size;
    uint64_t head;
    uint64_t tail;

    XTSpinlock lock;

    // Вместо XTThread* используем события
    XTEvent* canWriteEvent; 
    XTEvent* canReadEvent;

    bool writeClosed;
    bool readClosed;
    int  refCount;
} XTPipeData;

// Вспомогательные функции буфера остаются без изменений
static uint64_t pipeUsed(const XTPipeData* p) {
    if (p->head >= p->tail) return p->head - p->tail;
    return p->size - (p->tail - p->head);
}

static uint64_t pipeFree(const XTPipeData* p) {
    return p->size - pipeUsed(p);
}

static uint64_t pipeWriteToBuffer(XTPipeData* p, const void* src, uint64_t count) {
    uint64_t freeSpace = pipeFree(p);
    if (freeSpace == 0) return 0;
    if (count > freeSpace) count = freeSpace;

    uint64_t firstPart = p->size - p->head;
    if (firstPart > count) firstPart = count;

    xtCopyMem((char*)p->buffer + p->head, src, firstPart);
    if (count > firstPart) {
        xtCopyMem(p->buffer, (const char*)src + firstPart, count - firstPart);
    }
    p->head = (p->head + count) % p->size;
    return count;
}

static uint64_t pipeReadFromBuffer(XTPipeData* p, void* dst, uint64_t count) {
    uint64_t used = pipeUsed(p);
    if (used == 0) return 0;
    if (count > used) count = used;

    uint64_t firstPart = p->size - p->tail;
    if (firstPart > count) firstPart = count;

    xtCopyMem(dst, (char*)p->buffer + p->tail, firstPart);
    if (count > firstPart) {
        xtCopyMem((char*)dst + firstPart, p->buffer, count - firstPart);
    }
    p->tail = (p->tail + count) % p->size;
    return count;
}

// --- Реализация записи ---

static XTResult pipeWriteFile(XTFile* file, const void* data, uint64_t offset,
                               uint64_t count, uint64_t* written) {
    (void)offset;
    XTPipeData* p = (XTPipeData*)file->data;
    if (!p || !data || !written) return XT_INVALID_PARAMETER;
    *written = 0;

    const char* src = (const char*)data;
    uint64_t remaining = count;

    while (remaining > 0) {
        xtLockSpinlock(&p->lock);

        if (p->readClosed || p->writeClosed) {
            xtUnlockSpinlock(&p->lock);
            return XT_BROKEN_PIPE;
        }

        uint64_t free = pipeFree(p);
        if (free == 0) {
            xtUnlockSpinlock(&p->lock);
            if (file->flags & XT_FILE_MODE_NONBLOCK) {
                return *written > 0 ? XT_SUCCESS : XT_WOULD_BLOCK;
            }

            // Ждем события "можно писать"
            xtWaitForEvents(&p->canWriteEvent, 1, 0, UINT64_MAX, NULL);
            continue; 
        }

        uint64_t toWrite = remaining < free ? remaining : free;
        uint64_t wrote = pipeWriteToBuffer(p, src, toWrite);
        *written += wrote;
        src += wrote;
        remaining -= wrote;

        xtUnlockSpinlock(&p->lock);
        
        // Сигнализируем, что в пайпе появились данные для чтения
        xtSignalEvent(p->canReadEvent);
    }

    return XT_SUCCESS;
}

// --- Реализация чтения ---

static XTResult pipeReadFile(XTFile* file, void* data, uint64_t offset,
                              uint64_t count, uint64_t* read) {
    (void)offset;
    XTPipeData* p = (XTPipeData*)file->data;
    if (!p || !data || !read) return XT_INVALID_PARAMETER;
    *read = 0;

    char* dst = (char*)data;
    uint64_t remaining = count;

    while (remaining > 0) {
        xtLockSpinlock(&p->lock);

        if (p->readClosed) {
            xtUnlockSpinlock(&p->lock);
            return XT_BROKEN_PIPE;
        }

        uint64_t used = pipeUsed(p);
        if (used == 0) {
            if (p->writeClosed) {
                xtUnlockSpinlock(&p->lock);
                break; // EOF
            }
            xtUnlockSpinlock(&p->lock);

            if (file->flags & XT_FILE_MODE_NONBLOCK) {
                return *read > 0 ? XT_SUCCESS : XT_WOULD_BLOCK;
            }

            // Ждем события "можно читать"
            xtWaitForEvents(&p->canReadEvent, 1, 0, UINT64_MAX, NULL);
            continue;
        }

        uint64_t toRead = remaining < used ? remaining : used;
        uint64_t got = pipeReadFromBuffer(p, dst, toRead);
        *read += got;
        dst += got;
        remaining -= got;

        xtUnlockSpinlock(&p->lock);

        // Сигнализируем, что место в буфере освободилось
        xtSignalEvent(p->canWriteEvent);

        break; // Прочитали часть — возвращаем управление
    }

    return XT_SUCCESS;
}

// --- Закрытие и очистка ---

static XTResult pipeCloseFile(XTMountPoint* mp, XTFile* file) {
    (void)mp;
    XTPipeData* p = (XTPipeData*)file->data;
    if (!p) return XT_INVALID_PARAMETER;

    xtLockSpinlock(&p->lock);

    if (file->flags & XT_FILE_MODE_WRITE) {
        p->writeClosed = true;
        xtSignalEvent(p->canReadEvent); // Будим читателя для EOF
    }
    if (file->flags & XT_FILE_MODE_READ) {
        p->readClosed = true;
        xtSignalEvent(p->canWriteEvent); // Будим писателя для Broken Pipe
    }

    p->refCount--;
    bool last = (p->refCount == 0);
    xtUnlockSpinlock(&p->lock);

    if (last) {
        // Освобождаем события (предполагаем наличие функции деструктора или просто free)
        xtHeapFree(p->canWriteEvent);
        xtHeapFree(p->canReadEvent);
        xtHeapFree(p->buffer);
        xtHeapFree(p);
    }

    return XT_SUCCESS;
}

// Остальные функции (GetFileInfo, таблицы IO) остаются такими же...

static XTResult pipeGetFileInfo(XTFile* file, XTFileInfo* info) {
    if (!info) return XT_INVALID_PARAMETER;
    xtSetMem(info, 0, sizeof(XTFileInfo));
    xtCopyString(info->name, "pipe", 512);
    info->flags = XT_PATH_NODE_TYPE_FILE;
    return XT_SUCCESS;
}

static XTFileIO pipeWriteIO = { .WriteFile = pipeWriteFile, .CloseFile = pipeCloseFile, .GetFileInfo = pipeGetFileInfo };
static XTFileIO pipeReadIO = { .ReadFile = pipeReadFile, .CloseFile = pipeCloseFile, .GetFileInfo = pipeGetFileInfo };

XTResult XTEXPORT xtCreatePipe(XTFile** write, XTFile** read, uint64_t bufferSize) {
    if (!write || !read) return XT_INVALID_PARAMETER;
    if (bufferSize == 0) bufferSize = 4096;

    XTPipeData* p = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTPipeData), &p));
    xtSetMem(p, 0, sizeof(*p));

    p->size = bufferSize;
    XT_TRY(xtHeapAlloc(bufferSize, &p->buffer));
    
    // Инициализируем события
    XT_TRY(xtCreateEvent(&p->canWriteEvent));
    XT_TRY(xtCreateEvent(&p->canReadEvent));

    xtInitSpinlock(&p->lock);
    p->refCount = 2;

    // Создание структур XTFile...
    XT_TRY(xtHeapAlloc(sizeof(XTFile), write));
    xtSetMem(*write, 0, sizeof(XTFile));
    (*write)->IO = &pipeWriteIO;
    (*write)->data = p;
    (*write)->flags = XT_FILE_MODE_WRITE;

    XT_TRY(xtHeapAlloc(sizeof(XTFile), read));
    xtSetMem(*read, 0, sizeof(XTFile));
    (*read)->IO = &pipeReadIO;
    (*read)->data = p;
    (*read)->flags = XT_FILE_MODE_READ;

    return XT_SUCCESS;
}