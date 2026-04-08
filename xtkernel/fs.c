#include <xt/io.h>
#include <xt/list.h>
#include <xt/memory.h>
#include <xt/kernel.h>
#include <xt/string.h>

XTResult XTEXPORT xtWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t size, uint64_t* written) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->WriteFile == NULL) return XT_NOT_IMPLEMENTED;
    uint64_t tempWritten = 0;
    if (written == NULL) {
        written = &tempWritten;
    }

    return file->IO->WriteFile(file, data, offset, size, written);

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

XTResult xtFileSystemInit() {
    //Create root
    return xtCreatePathNode("", &rootMP, &root);
}

XTResult XTEXPORT xtOpenFile(const char* path, uint64_t flags, XTFile** out) {
    XT_CHECK_ARG_IS_NULL(path);
    XT_CHECK_ARG_IS_NULL(out);
    XTPathNode* pathNode = NULL;
    char* left = NULL;
    XT_TRY(xtFindPathNode(path, &pathNode, &left));
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

    return pathNode->mp->fs->IO->OpenFile(pathNode->mp, left, flags & ~(XT_FILE_MODE_CREATE), out);
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

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;

    if (file->IO->ReadFile == NULL) return XT_NOT_IMPLEMENTED;
    
    uint64_t tempRead = 0;

    if (read == NULL) {
        read = &tempRead;
    }

    return file->IO->ReadFile(file, data, offset, size, read);
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
