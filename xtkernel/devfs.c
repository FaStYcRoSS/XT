#include <xt/io.h>
#include <xt/list.h>
#include <xt/string.h>
#include <xt/memory.h>

XTList* devices = NULL;
XTSpinlock devLock;

typedef struct XTDevice {
    char* name;
    XTFile* file;
} XTDevice;

XTResult xtFindDevice(const char* name, XTDevice** device) {
    xtLockSpinlock(&devLock);
    for (XTList* i = devices; i; xtGetNextList(i, &i)) {
        XTDevice* dev = NULL;
        xtGetListData(i, &dev);
        if (xtStringICmp(dev->name,name, 4096) == XT_SUCCESS) {
            *device = dev;
            xtUnlockSpinlock(&devLock);
            return XT_SUCCESS;
        }
    }
    xtUnlockSpinlock(&devLock);
    return XT_NOT_FOUND;
}

XTResult xtRegisterDevice(const char* name, XTFile* file) {
    XT_CHECK_ARG_IS_NULL(name);
    XT_CHECK_ARG_IS_NULL(file);
    XTDevice* dev = NULL;
    XTResult result = 0;
    result = xtFindDevice(name, &dev);
    if (result == XT_SUCCESS) {
        return XT_FILE_ALREADY_EXISTS;
    }
    XT_TRY(xtHeapAlloc(sizeof(XTDevice), &dev));

    result = xtDuplicateString(name, &dev->name);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(dev);
        return result;
    } 
    XTFile* newFile = NULL;
    result = xtHeapAlloc(sizeof(XTFile), &newFile);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(dev->name);
        xtHeapFree(dev);
        return result;
    }
    xtCopyMem(newFile, file, sizeof(XTFile));
    dev->file = newFile;
    XTList* list = NULL;
    result = xtCreateList(dev, &list);
    if (XT_IS_ERROR(result)) {
        xtHeapFree(dev->file);
        xtHeapFree(dev->name);
        xtHeapFree(dev);
        return XT_SUCCESS;
    }
    xtLockSpinlock(&devLock);
    if (devices == NULL) {
        devices = list;
    }
    else {
        xtAppendList(devices, list);
    }
    xtUnlockSpinlock(&devLock);
    return XT_SUCCESS;
}


XTResult xtUnregisterDevice(const char* name) {
    XT_CHECK_ARG_IS_NULL(name);
    XTList* list = NULL;
    xtLockSpinlock(&devLock);
    for (XTList* i = devices; i; xtGetNextList(i, &i)) {
        XTDevice* dev = NULL;
        xtGetListData(i, &dev);
        if (xtStringICmp(dev->name,name, 4096) == XT_SUCCESS) {
            list = i;
            xtRemoveFromList(devices, list);
            xtUnlockSpinlock(&devLock);
            return XT_SUCCESS;
        }
    }
    xtUnlockSpinlock(&devLock);
    if (list == NULL) {
        return XT_NOT_FOUND;
    }
}

XTResult devfsOpenFile(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** file) {
    XTDevice* dev = NULL;
    // Ищем устройство в твоем списке
    XT_TRY(xtFindDevice(name, &dev));

    // 1. Создаем НОВЫЙ дескриптор файла для этого конкретного "открытия"
    XTFile* newFile = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTFile), &newFile));
    xtSetMem(newFile, 0, sizeof(XTFile));

    // 2. Копируем функции ввода-вывода из мастер-файла устройства
    newFile->IO = dev->file->IO;
    
    // 3. Копируем указатель на данные (контекст драйвера)
    newFile->data = dev->file->data;

    // 4. Настраиваем индивидуальные параметры
    newFile->flags = flags;
    newFile->mountPoint = mp;

    // 5. ОЧЕНЬ ВАЖНО: Референс-каунтинг
    // Если твой драйвер (например, pipe) уничтожает данные в CloseFile, 
    // тебе нужно поле refCount в структурах данных, чтобы не освободить 
    // память, пока кто-то еще держит файл открытым.
    
    *file = newFile;
    return XT_SUCCESS;
}

XTResult devfsMount(XTMountPoint* mp) {
    return XT_SUCCESS;
}

XTResult devfsUnmount(XTMountPoint* mp) {
    return XT_SUCCESS;
}

XTFileSystemIO devfsIO = {
    .OpenFile = devfsOpenFile,
    .Mount = devfsMount,
    .Unmount = devfsUnmount
};

XTFileSystem devfsInstance = {
    .IO = &devfsIO,
    .Name = "devfs"
};

XTResult xtDeviceFilesystemInit() {
    XT_TRY(xtInitSpinlock(&devLock));
    XT_TRY(xtRegisterFileSystem(&devfsInstance));
    return XT_SUCCESS;
}


