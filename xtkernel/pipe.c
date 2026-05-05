#include <xt/io.h>
#include <xt/kernel.h>
#include <xt/string.h>
#include <xt/memory.h>
#include <stdbool.h>

// -------------------------------------------------------------------
// Структура данных pipe (общая для обоих концов)
// -------------------------------------------------------------------
typedef struct XTPipeData {
    void*    buffer;
    uint64_t size;
    uint64_t head;
    uint64_t tail;

    XTSpinlock lock;           // простой спинлок вместо RWLock

    XTThread* writeWaitThread;
    XTThread* readWaitThread;

    bool writeClosed;
    bool readClosed;
    int  refCount;
} XTPipeData;

// Вспомогательные функции для кольцевого буфера
static uint64_t pipeUsed(const XTPipeData* p) {
    if (p->head >= p->tail)
        return p->head - p->tail;
    else
        return p->size - (p->tail - p->head);
}

static uint64_t pipeFree(const XTPipeData* p) {
    return p->size - pipeUsed(p);
}

// Копирование данных в кольцевой буфер (без блокировки, только внутренняя логика)
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

// Копирование данных из кольцевого буфера (без блокировки)
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

// -------------------------------------------------------------------
// Функции-заглушки для работы с потоками (будут заменены вызывающим)
// -------------------------------------------------------------------
XTResult xtGetCurrentThread(XTThread* thread);
XTResult xtWaitThread(XTThread* thread);
XTResult xtWakeUpThread(XTThread* thread);

// -------------------------------------------------------------------
// Реализация XTFileIO для конца ЗАПИСИ
// -------------------------------------------------------------------

static XTResult pipeWriteFile(XTFile* file, const void* data, uint64_t offset,
                               uint64_t count, uint64_t* written) {
    (void)offset;
    XTPipeData* p = (XTPipeData*)file->data;
    if (!p || !data || !written) return XT_INVALID_PARAMETER;
    *written = 0;
    if (count == 0) return XT_SUCCESS;

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
            if (file->flags & XT_FILE_MODE_NONBLOCK) {
                xtUnlockSpinlock(&p->lock);
                return *written > 0 ? XT_SUCCESS : XT_WOULD_BLOCK;
            }
            // Атомарно: регистрируемся как ждущий и отпускаем лок
            XTThread* current = NULL;
            xtGetCurrentThread(&current);
            p->writeWaitThread = current;
            xtUnlockSpinlock(&p->lock);
            // xtSleepThread сам переводит в WAIT и переключает планировщик
            xtSleepThread(current, UINT64_MAX);
            continue;
        }

        uint64_t toWrite = remaining < free ? remaining : free;
        uint64_t wrote = pipeWriteToBuffer(p, src, toWrite);
        *written += wrote;
        src += wrote;
        remaining -= wrote;

        XTThread* toWake = p->readWaitThread;
        p->readWaitThread = NULL;
        xtUnlockSpinlock(&p->lock);

        if (toWake) xtWakeUpThread(toWake);
    }

    return XT_SUCCESS;
}

// -------------------------------------------------------------------
// Реализация XTFileIO для конца ЧТЕНИЯ
// -------------------------------------------------------------------
static XTResult pipeReadFile(XTFile* file, void* data, uint64_t offset,
                              uint64_t count, uint64_t* read) {
    (void)offset;
    XTPipeData* p = (XTPipeData*)file->data;
    if (!p || !data || !read) return XT_INVALID_PARAMETER;
    *read = 0;
    if (count == 0) return XT_SUCCESS;

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
            // EOF — писатель закрыт и данных нет
            if (p->writeClosed) {
                xtUnlockSpinlock(&p->lock);
                break;
            }
            if (file->flags & XT_FILE_MODE_NONBLOCK) {
                xtUnlockSpinlock(&p->lock);
                return *read > 0 ? XT_SUCCESS : XT_WOULD_BLOCK;
            }
            XTThread* current = NULL;
            xtGetCurrentThread(&current);
            p->readWaitThread = current;
            xtUnlockSpinlock(&p->lock);
            xtSleepThread(current, UINT64_MAX);
            continue;
        }

        uint64_t toRead = remaining < used ? remaining : used;
        uint64_t got = pipeReadFromBuffer(p, dst, toRead);
        *read += got;
        dst += got;
        remaining -= got;

        XTThread* toWake = p->writeWaitThread;
        p->writeWaitThread = NULL;
        xtUnlockSpinlock(&p->lock);

        if (toWake) xtWakeUpThread(toWake);

        // Если прочитали хоть что-то — не ждём остального
        break;
    }

    return XT_SUCCESS;
}

// -------------------------------------------------------------------
// CloseFile – вызывается при закрытии любого конца
// -------------------------------------------------------------------
static XTResult pipeCloseFile(XTMountPoint* mp, XTFile* file) {
    (void)mp;
    XTPipeData* p = (XTPipeData*)file->data;
    if (!p) return XT_INVALID_PARAMETER;

    xtLockSpinlock(&p->lock);

    // Определяем, какой конец закрывается
    if (file->flags & XT_FILE_MODE_WRITE) {
        p->writeClosed = true;
        // Будим читателя, чтобы он увидел EOF
        if (p->readWaitThread) {
            xtWakeUpThread(p->readWaitThread);
            p->readWaitThread = NULL;
        }
    }
    if (file->flags & XT_FILE_MODE_READ) {
        p->readClosed = true;
        // Будим писателя, чтобы он получил ошибку broken pipe
        if (p->writeWaitThread) {
            xtWakeUpThread(p->writeWaitThread);
            p->writeWaitThread = NULL;
        }
    }

    p->refCount--;
    bool last = (p->refCount == 0);
    xtUnlockSpinlock(&p->lock);

    if (last) {
        // Освобождаем общие ресурсы
        xtHeapFree(p->buffer);
        xtHeapFree(p);
    }

    return XT_SUCCESS;
}

// Заглушка для GetFileInfo
static XTResult pipeGetFileInfo(XTFile* file, XTFileInfo* info) {
    (void)file;
    if (!info) return XT_INVALID_PARAMETER;
    xtSetMem(info, 0, sizeof(XTFileInfo));
    // Можно заполнить имя, тип и т.д.
    xtCopyString(info->name, "pipe", 512);
    info->flags = XT_PATH_NODE_TYPE_FILE;
    return XT_SUCCESS;
}

// -------------------------------------------------------------------
// Таблицы виртуальных функций для двух концов
// -------------------------------------------------------------------
static XTFileIO pipeWriteIO = {
    .WriteFile   = pipeWriteFile,
    .ReadFile    = NULL,          // чтение из конца записи запрещено
    .DeviceIO    = NULL,
    .MapFile     = NULL,
    .UnmapFile   = NULL,
    .CloseFile   = pipeCloseFile,
    .GetFileInfo = pipeGetFileInfo,
    .SetFileInfo = NULL
};

static XTFileIO pipeReadIO = {
    .WriteFile   = NULL,           // запись в конец чтения запрещена
    .ReadFile    = pipeReadFile,
    .DeviceIO    = NULL,
    .MapFile     = NULL,
    .UnmapFile   = NULL,
    .CloseFile   = pipeCloseFile,
    .GetFileInfo = pipeGetFileInfo,
    .SetFileInfo = NULL
};

// -------------------------------------------------------------------
// Пользовательская функция создания pipe
// -------------------------------------------------------------------
XTResult XTEXPORT xtCreatePipe(XTFile** write, XTFile** read, uint64_t bufferSize) {
    if (!write || !read) return XT_INVALID_PARAMETER;
    if (bufferSize == 0) bufferSize = 4096;

    // Выделяем общую структуру данных
    XTPipeData* p = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTPipeData), &p));
    if (!p) return XT_OUT_OF_MEMORY;
    xtSetMem(p, 0, sizeof(*p));

    p->size = bufferSize;
    XT_TRY(xtHeapAlloc(bufferSize, &p->buffer));
    if (!p->buffer) {
        xtHeapFree(p);
        return XT_OUT_OF_MEMORY;
    }
    p->head = p->tail = 0;
    xtInitSpinlock(&p->lock);
    p->writeWaitThread = NULL;
    p->readWaitThread = NULL;
    p->writeClosed = false;
    p->readClosed = false;
    p->refCount = 2;   // два конца – write и read

    // Создаём XTFile для записи
    XTFile* w = NULL;
    XTResult result = xtHeapAlloc(sizeof(XTFile), &w);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(p->buffer);
        xtHeapFree(p);
        return XT_OUT_OF_MEMORY;
    }
    xtSetMem(w, 0, sizeof(*w));
    w->mountPoint = NULL;           // pipe не привязан к ФС
    w->IO = &pipeWriteIO;
    w->data = p;
    w->flags = XT_FILE_MODE_WRITE;   // или дополнительно можно установить XT_FILE_MODE_NONBLOCK по желанию
    xtInitSpinlock(&w->lock);

    // Создаём XTFile для чтения
    XTFile* r = NULL;
    result = xtHeapAlloc(sizeof(XTFile), &r);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(p->buffer);
        xtHeapFree(p);
        return XT_OUT_OF_MEMORY;
    }
    xtSetMem(r, 0, sizeof(*r));
    r->mountPoint = NULL;
    r->IO = &pipeReadIO;
    r->data = p;
    r->flags = XT_FILE_MODE_READ;
    xtInitSpinlock(&r->lock);

    *write = w;
    *read = r;
    return XT_SUCCESS;
}