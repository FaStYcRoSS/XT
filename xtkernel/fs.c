#include <xt/io.h>
#include <xt/list.h>
#include <xt/memory.h>
#include <xt/kernel.h>
#include <xt/string.h>
#include <xt/sharedPtr.h>

XTResult XTEXPORT xtWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t size, uint64_t* written) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->WriteFile == NULL) return XT_NOT_IMPLEMENTED;
    uint64_t tempWritten = 0;
    if (written == NULL) {
        written = &tempWritten;
    }
    XT_TRY(xtLockFile(file, XT_LOCK_WRITE));
    XTResult result = file->IO->WriteFile(file, data, offset, size, written);
    xtSignalEvent(file->event);
    xtUnlockFile(file, XT_LOCK_WRITE);
    return result;
}

XTResult XTEXPORT xtGetFileInfo(XTFile* file, XTFileInfo* info) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(info);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->GetFileInfo == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->GetFileInfo(file, info);
}

XTResult XTEXPORT xtSetFileInfo(XTFile* file, XTFileInfo* info) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(info);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->SetFileInfo == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->SetFileInfo(file, info);
}


XTResult XTEXPORT xtReadDirectory(XTDirectory* dir, XTFileInfo* info) {
    XT_CHECK_ARG_IS_NULL(dir);
    XT_CHECK_ARG_IS_NULL(info);
    XTList* l = NULL;
    XTResult result = xtIndexList(dir->node->nodes, dir->pos, &l);
    if (XT_IS_ERROR(result)) {
        uint64_t listLength = 0;
        xtListLength(dir->node->nodes, &listLength);
        dir->pos -= listLength;
        XTResult result = dir->node->mp->fs->IO->ReadDirectory(dir, info);
        dir->pos += listLength + 1;
        return result;
    }
    XTPathNode* lNode = NULL;
    XT_TRY(xtGetListData(l, &lNode));
    xtCopyString(info->name, lNode->name, 256);
    ++dir->pos;
    return XT_SUCCESS;
}


XTPathNode* root = NULL;

XTList* filesystems = NULL;

XTResult XTEXPORT xtRegisterFileSystem(XTFileSystem* fs) {

    XTList* fsList = NULL;
    XT_TRY(xtCreateList(fs, &fsList));
    if (filesystems == NULL) {
        filesystems = fsList;
        return XT_SUCCESS;
    }
    XT_TRY(xtAppendList(filesystems, fsList));
    return XT_SUCCESS;
}

XTResult xtFindPathNode(const char* path, XTPathNode** node, char** left) {
    XTPathNode* pathi = root;
    for (;*path; ++path) {
        int found = 0;
        char name[256];
        char* i = name;
        const char* prevPath = path;
        for (
            uint64_t count = 0;
            *path != '/' && *path != '\\' && *path && count < 255;
            ++i, ++path, ++count
        ) {
            *i = *path;
        }
        *i = '\0';
        if (name[0] == '\0') {
            *left = (char*)(path + 1); // Если встретили слэш, обновляем left
            continue;
        }
        for (XTList* pathl = pathi->nodes; pathl; xtGetNextList(pathl, &pathl)) {
            XTPathNode* _node = NULL;
            xtGetListData(pathl, &_node);
            if (xtStringCmp(_node->name, name, 256) == XT_SUCCESS) {
                pathi = _node;
                found = 1;
                break;
            }
        }
        if (found) {
            *left = (char*)path; // Обновляем left при переходе вглубь
            continue;
        }
        *left = prevPath;
        break;
    }
    *node = pathi;
    return XT_SUCCESS;

}

XTResult xtNormalizePath(const char* path, char* buffer, uint64_t buffsize) {
    XT_CHECK_ARG_IS_NULL(path);
    XT_CHECK_ARG_IS_NULL(buffer);
    if (buffsize == 0) return XT_INVALID_PARAMETER;

    // Абсолютный путь должен начинаться с '/' или '\'
    if (*path != '/' && *path != '\\')
        return XT_INVALID_PARAMETER;

    // Начинаем с корневого слеша
    buffer[0] = '/';
    buffer[1] = '\0';
    uint64_t len = 1; // текущая длина нормализованного пути (без завершающего нуля)

    while (*path) {
        // Пропускаем все разделители (и прямые, и обратные)
        while (*path == '/' || *path == '\\')
            ++path;
        if (*path == '\0')
            break; // завершающие разделители игнорируем

        // Извлекаем очередной компонент (до следующего разделителя или конца)
        char name[256];
        uint64_t comp_len = 0;
        while (*path && *path != '/' && *path != '\\' && comp_len < sizeof(name) - 1) {
            name[comp_len++] = *path++;
        }
        name[comp_len] = '\0';

        // Пустой компонент (два слеша подряд) – пропускаем
        if (comp_len == 0)
            continue;

        // Обрабатываем "." – ничего не делаем
        if (xtStringICmp(name, ".", 4096) == 0)
            continue;

        // Обрабатываем ".." – поднимаемся на уровень выше
        if (xtStringICmp(name, "..", 4096) == 0) {
            if (len > 1) {
                // Ищем последний слеш в текущем нормализованном пути
                uint64_t pos = len - 1;
                while (pos > 0 && buffer[pos] != '/')
                    --pos;
                if (pos == 0) {
                    // Корень – обрезаем до одного слеша
                    buffer[1] = '\0';
                    len = 1;
                } else {
                    // Обрезаем строку после найденного слеша
                    buffer[pos] = '\0';
                    len = pos;
                }
            }
            // Если уже в корне – ничего не делаем
            continue;
        }

        // Обычный компонент – добавляем в буфер
        // Если это не первый компонент (len > 1), ставим перед ним слеш
        if (len > 1) {
            if (len + 1 >= buffsize)
                return XT_OUT_OF_BOUNDARY; // недостаточно места
            buffer[len++] = '/';
        }
        // Копируем имя компонента
        if (len + comp_len >= buffsize)
            return XT_OUT_OF_BOUNDARY;
        for (uint64_t i = 0; i < comp_len; ++i)
            buffer[len++] = name[i];
        buffer[len] = '\0';
    }

    // Убираем завершающий слеш (кроме корня)
    if (len > 1 && buffer[len - 1] == '/') {
        buffer[len - 1] = '\0';
        len--;
    }

    return XT_SUCCESS;
}

XTResult xtCreatePathNode(const char* name, XTMountPoint* mp, XTPathNode** out) {
    XT_CHECK_ARG_IS_NULL(name);
    XTPathNode* node = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTPathNode), &node));
    const char* dupname = NULL;
    XT_TRY(xtDuplicateString(name, &dupname));
    node->name = dupname;
    node->mp = mp;
    node->nodes = NULL;
    *out = node;
    return XT_SUCCESS;
}

XTResult xtAppendPathNode(XTPathNode* parent, XTPathNode* child) {
    XT_CHECK_ARG_IS_NULL(parent);
    XT_CHECK_ARG_IS_NULL(child);
    XTList* childList = NULL;
    xtCreateList(child, &childList);
    if (parent->nodes == NULL) parent->nodes = childList;
    else xtAppendList(parent->nodes, childList);
    return XT_SUCCESS;
}

XTMountPoint rootMP = {
    .data = NULL,
    .device = NULL,
    .fs = NULL
};

XTSpinlock openedFilesLock;
XTList* openedFiles = NULL;

typedef struct XTOpenedFileEntry {
    char path[4096];
    XTFile* file;
} XTOpenedFileEntry;

XTResult xtFileSystemInit() {
    //Create root
    xtInitSpinlock(&openedFilesLock);
    return xtCreatePathNode("", &rootMP, &root);
}

XTResult xtFindFile(const char* path, XTFile** out) {
    xtLockSpinlock(&openedFilesLock);
    for (XTList* i = openedFiles; i; xtGetNextList(i, &i)) {
        XTSharedPtr* sharedPtr = NULL;
        xtGetListData(i, &sharedPtr);
        XTOpenedFileEntry* openedFileEntry = NULL;
        xtSharedPtrGetData(sharedPtr, &openedFileEntry);
        if (xtStringICmp(path, openedFileEntry->path, 4096) == XT_SUCCESS) {
            xtIncrementReference(sharedPtr);
            *out = openedFileEntry->file;
            xtUnlockSpinlock(&openedFilesLock);
            return XT_SUCCESS;
        }
    }
    xtUnlockSpinlock(&openedFilesLock);
    return XT_NOT_FOUND;
}

XTResult xtAppendFile(const char* path, XTFile* file) {
    XTOpenedFileEntry *fileEntry = NULL;
    xtHeapAlloc(sizeof(XTOpenedFileEntry), &fileEntry);
    xtCopyString(fileEntry->path, path, 4096);
    fileEntry->file = file;
    XTSharedPtr *sharedPtr = NULL;
    xtCreateSharedPtr(fileEntry, &sharedPtr, NULL);
    XTList* fileEntryList = NULL;
    xtCreateList(sharedPtr, &fileEntryList);
    xtLockSpinlock(&openedFilesLock);
    if (openedFiles == NULL) {
        openedFiles = fileEntryList;
    }
    else {
        xtAppendList(openedFiles, fileEntryList);
    }
    xtUnlockSpinlock(&openedFilesLock);
}

XTResult XTEXPORT xtOpenFile(const char* path, uint64_t flags, XTFile** out) {
    XT_CHECK_ARG_IS_NULL(path);
    XT_CHECK_ARG_IS_NULL(out);
    XTPathNode* pathNode = NULL;
    char* left = NULL;
    char buff[4096];
    XT_TRY(xtNormalizePath(path, buff, 4096));
    XTResult result = xtFindFile(buff, out);
    if (result == XT_SUCCESS) return XT_SUCCESS;
    XT_TRY(xtFindPathNode(buff, &pathNode, &left));
    if (pathNode == NULL) {
        xtDebugPrint("XT_ERROR: pathNode is NULL\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp is NULL (MountPoint not found)\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp->fs == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp->fs is NULL (FileSystem not initialized)\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp->fs->IO == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp->fs->IO is NULL (IO Operations table missing)\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp->fs->IO->OpenFile == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp->fs->IO->OpenFile is NULL\n");
        return XT_NOT_IMPLEMENTED;
    }

    if (flags & XT_FILE_MODE_CREATE) {
        if (pathNode->mp->fs->IO->CreateFile == NULL) {
            xtDebugPrint("XT_ERROR: Create flag set, but IO->CreateFile is NULL\n");
            return XT_NOT_IMPLEMENTED;
        }
        pathNode->mp->fs->IO->CreateFile(pathNode->mp, left, flags & ~(XT_FILE_MODE_CREATE));
    }

    result = pathNode->mp->fs->IO->OpenFile(pathNode->mp, left, flags & ~(XT_FILE_MODE_CREATE), out);
    if (XT_IS_ERROR(result)) return result;
    xtInitRWLock(&(*out)->lock);
    (*out)->flags = flags & ~(XT_FILE_MODE_CREATE);
    xtCreateEvent(&(*out)->event);
    return result;
}


XTResult XTEXPORT xtLockFile(XTFile* file, uint64_t lockType) {
    XT_CHECK_ARG_IS_NULL(file);

    XTRWLock* lock = &file->lock;
    if (lock == NULL) return XT_NOT_IMPLEMENTED;

    // Неблокирующий режим: проверяем без ожидания
    if (file->flags & XT_FILE_MODE_NONBLOCK) {
        xtLockSpinlock(&lock->spinlock);
        int canLock = (lockType == XT_LOCK_READ)
            ? (lock->readers >= 0 && lock->waitWriters == NULL)
            : (lock->readers == 0);
        xtUnlockSpinlock(&lock->spinlock);
        if (!canLock) return XT_WOULD_BLOCK;  // аналог EWOULDBLOCK
    }

    if (lockType == XT_LOCK_READ) {
        return xtAcquireRead(lock);
    } else if (lockType == XT_LOCK_WRITE) {
        return xtAcquireWrite(lock);
    }
    return XT_INVALID_PARAMETER;
}

XTResult XTEXPORT xtUnlockFile(XTFile* file, uint64_t lockType) {
    XT_CHECK_ARG_IS_NULL(file);

    XTRWLock* lock = &file->lock;
    if (lock == NULL) return XT_NOT_IMPLEMENTED;

    if (lockType == XT_LOCK_READ) {
        return xtReleaseRead(lock);
    } else if (lockType == XT_LOCK_WRITE) {
        return xtReleaseWrite(lock);
    }
    return XT_INVALID_PARAMETER;
}

XTResult XTEXPORT xtOpenDirectory(const char* path, XTDirectory** out) {
    XT_CHECK_ARG_IS_NULL(path);
    XT_CHECK_ARG_IS_NULL(out);
    XTPathNode* pathNode = NULL;
    char* left = NULL;
    XT_TRY(xtFindPathNode(path, &pathNode, &left));
    XTDirectory* newDir = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTDirectory), &newDir));
    newDir->node = pathNode;
    newDir->pos = 0;
    if (pathNode->mp == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp->fs == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp->fs->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp->fs->IO->OpenDirectory == NULL) return XT_NOT_IMPLEMENTED;
    XTResult result = pathNode->mp->fs->IO->OpenDirectory(pathNode->mp, left, newDir);
    *out = newDir;
    return result;
}

XTResult XTEXPORT xtMakeFS(XTFile* file, const char* filesystem) {
    XT_CHECK_ARG_IS_NULL(filesystem);
    XT_CHECK_ARG_IS_NULL(file);
    XTFileSystem* fs = NULL;
    for (XTList* l = filesystems; l; xtGetNextList(l, &l)) {
        XTFileSystem* fsI = NULL;
        xtGetListData(l, &fsI);
        if (xtStringCmp(fsI->Name, filesystem, 256) == XT_SUCCESS) {
            fs = fsI;
        }
    }
    return fs->IO->MakeFS(file);
}

XTResult XTEXPORT xtCopyFile(const char* source, const char* dest) {
    XTFile* fsource = NULL;
    XTFile* fdest = NULL;
    XT_TRY(xtOpenFile(source, XT_FILE_MODE_READ, &fsource));
    XT_TRY(xtOpenFile(dest, XT_FILE_MODE_WRITE, &fdest));
    char buff[512];
    XTFileInfo info = { 0 };
    xtGetFileInfo(fsource, &info);
    uint64_t count = info.FileSize;
    uint64_t pos = 0;
    while (count) {
        uint64_t temp = count < 512 ? count : 512;
        xtReadFile(fsource, buff, pos, temp, NULL);
        xtWriteFile(fdest, buff, pos, temp, NULL);
        xtSetMem(buff, 0, 512);
        pos += temp;
        count -= temp;
    }
    xtCloseFile(fsource);
    xtCloseFile(fdest);
    return XT_SUCCESS;
}

XTResult XTEXPORT xtCloseFile(XTFile* file) {
    XT_CHECK_ARG_IS_NULL(file);
    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->CloseFile == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->CloseFile(file->mountPoint, file);
}

XTResult xtCheckName(const char* path) {
    XT_CHECK_ARG_IS_NULL(path);
    xtDebugPrint("path %s\n", path);
    for (;*path;++path) {
        if (*path == '/' || *path == '\\') return XT_BAD_NAME;
    }
    return XT_SUCCESS;
}

XTResult XTEXPORT xtMount(const char* path, const char* filesystemName, XTFile* dev) {

    XT_CHECK_ARG_IS_NULL(path);
    XT_CHECK_ARG_IS_NULL(filesystemName);

    XTFileSystem* fs = NULL;
    for (XTList* l = filesystems; l; xtGetNextList(l, &l)) {
        XTFileSystem* fsI = NULL;
        xtGetListData(l, &fsI);
        if (xtStringCmp(fsI->Name, filesystemName, 256) == XT_SUCCESS) {
            fs = fsI;
        }
    }
    if (fs == NULL) {
        xtDebugPrint("fs not found: %s\n", filesystemName);
        return XT_NOT_FOUND;
    }
    char* left = NULL;
    XTPathNode* pathNode = NULL;
    XTPathNode* newPath = NULL;
    XTMountPoint* mp = NULL;
    if (root != NULL) {
        XT_TRY(xtFindPathNode(path, &pathNode, &left));
        if (left == NULL || *left == '\0') {
            pathNode->mp->fs = fs;
            pathNode->mp->device = dev;
            return pathNode->mp->fs->IO->Mount(pathNode->mp); // Вызываем Mount для этого узла
        }
        XT_TRY(xtCheckName(left));
    }
    XT_TRY(xtHeapAlloc(sizeof(XTMountPoint), &mp));
    mp->fs = fs;
    mp->device = dev;
    if (mp->fs->IO->Mount == NULL) return XT_NOT_IMPLEMENTED;
    XTResult resultOfMount = mp->fs->IO->Mount(mp);
    if (left == NULL) {
        left = "";
    }
    XT_TRY(xtCreatePathNode(left, mp, &newPath));
    if (XT_IS_ERROR(resultOfMount)) {
        xtHeapFree(mp);
        return resultOfMount;
    }
    if (root == NULL) {
        root = newPath;
    }
    else {
        xtAppendPathNode(pathNode, newPath);
    }
    return XT_SUCCESS;
    
}

XTResult xtUnmount(const char* path) {
    XTPathNode* pathNode = NULL;
    char* left = NULL;
    XT_TRY(xtFindPathNode(path, &pathNode, &left));
    if (left != NULL) return XT_NOT_FOUND;
    pathNode->mp->fs->IO->Unmount(pathNode->mp);

}

XTResult XTEXPORT xtReadFile(XTFile* file, void* data, uint64_t offset, uint64_t size, uint64_t* read) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);
    xtResetEvent(file->event);
    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;

    if (file->IO->ReadFile == NULL) return XT_NOT_IMPLEMENTED;
    
    uint64_t tempRead = 0;

    if (read == NULL) {
        read = &tempRead;
    }

    XT_TRY(xtLockFile(file, XT_LOCK_READ));
    XTResult result = file->IO->ReadFile(file, data, offset, size, read);
    xtUnlockFile(file, XT_LOCK_READ);
    return result;
}

XTResult XTEXPORT xtMapFile(XTFile* file, uint64_t offset, uint64_t* size, void** out) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(size);
    XT_CHECK_ARG_IS_NULL(out);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->MapFile == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->MapFile(file, offset, size, out);

}

XTResult XTEXPORT xtUnmapFile(XTFile* file, uint64_t offset, void* ptr, uint64_t size) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(size);
    XT_CHECK_ARG_IS_NULL(ptr);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->UnmapFile == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->UnmapFile(file, offset, ptr, size);
}

XTResult XTEXPORT xtDeleteFile(const char* path) {
    XTPathNode* pathNode = NULL;
    char* left = NULL;
    char buff[4096];
    XT_TRY(xtNormalizePath(path, buff, 4096));
    XT_TRY(xtFindPathNode(buff, &pathNode, &left));
    if (pathNode == NULL) {
        xtDebugPrint("XT_ERROR: pathNode is NULL\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp is NULL (MountPoint not found)\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp->fs == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp->fs is NULL (FileSystem not initialized)\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp->fs->IO == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp->fs->IO is NULL (IO Operations table missing)\n");
        return XT_NOT_IMPLEMENTED;
    }
    if (pathNode->mp->fs->IO->DeleteFile == NULL) {
        xtDebugPrint("XT_ERROR: pathNode->mp->fs->IO->OpenFile is NULL\n");
        return XT_NOT_IMPLEMENTED;
    }
    XTResult result = pathNode->mp->fs->IO->DeleteFile(pathNode->mp, left);
    return result;
}
