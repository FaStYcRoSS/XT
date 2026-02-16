#include <xt/io.h>
#include <xt/list.h>
#include <xt/memory.h>
#include <xt/kernel.h>
#include <xt/string.h>

XTResult xtWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t size, uint64_t* written) {
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

typedef struct XTPathNode {
    XTMountPoint* mp;
    const char* name;
    XTList*     nodes;
} XTPathNode;

XTPathNode* root = NULL;

XTList* filesystems = NULL;

XTResult xtRegisterFileSystem(XTFileSystem* fs) {

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
        if (name[0] == '\0') continue;
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
            continue;
        }
        *left = prevPath;
        break;
    }
    if (*path == '\0') *left = NULL;
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

XTResult xtFileSystemInit() {

    return XT_SUCCESS;
}

XTResult xtOpenFile(const char* path, uint64_t flags, XTFile** out) {
    XTPathNode* pathNode = NULL;
    char* left = NULL;
    XT_TRY(xtFindPathNode(path, &pathNode, &left));
    if (pathNode->mp == NULL) return XT_NOT_FOUND;
    if (flags & XT_FILE_MODE_CREATE) {
        pathNode->mp->fs->IO->CreateFile(pathNode->mp, left, flags & ~(XT_FILE_MODE_CREATE));
    }
    if (pathNode == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp->fs == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp->fs->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (pathNode->mp->fs->IO->OpenFile == NULL) return XT_NOT_IMPLEMENTED;
    return pathNode->mp->fs->IO->OpenFile(pathNode->mp, left, flags & ~(XT_FILE_MODE_CREATE), out);
}

XTResult xtCloseFile(XTFile* file) {
    return file->IO->CloseFile(file->mountPoint, file);
}

XTResult xtCheckName(const char* path) {
    for (;*path;++path) {
        if (*path == '/' || *path == '\\') return XT_BAN_NAME;
    }
    return XT_SUCCESS;
}

XTResult xtMount(const char* path, const char* filesystemName, XTFile* dev) {

    XT_CHECK_ARG_IS_NULL(path);
    XT_CHECK_ARG_IS_NULL(filesystemName);
    XT_CHECK_ARG_IS_NULL(dev);

    XTFileSystem* fs = NULL;
    for (XTList* l = filesystems; l; xtGetNextList(l, &l)) {
        XTFileSystem* fsI = NULL;
        xtGetListData(l, &fsI);
        if (xtStringCmp(fsI->Name, filesystemName, 256) == XT_SUCCESS) {
            fs = fsI;
        }
    }

    if (fs == NULL) return XT_NOT_FOUND;
    char* left = NULL;
    XTPathNode* pathNode = NULL;
    if (root != NULL) {
        XT_TRY(xtFindPathNode(path, &pathNode, &left));
        XT_TRY(xtCheckName(left));
    }
    XTPathNode* newPath = NULL;
    XTMountPoint* mp = NULL;
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

XTResult xtReadFile(XTFile* file, void* data, uint64_t offset, uint64_t size, uint64_t* read) {
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

XTResult xtMapFile(XTFile* file, uint64_t offset, uint64_t* size, void** out) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(size);
    XT_CHECK_ARG_IS_NULL(out);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->MapFile == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->MapFile(file, offset, size, out);

}

XTResult xtUnmapFile(XTFile* file, uint64_t offset, void* ptr, uint64_t size) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(size);
    XT_CHECK_ARG_IS_NULL(ptr);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;
    if (file->IO->UnmapFile == NULL) return XT_NOT_IMPLEMENTED;
    return file->IO->UnmapFile(file, offset, ptr, size);
}

XTResult xtWriteToBuffer(XTFile* file, const void* data, uint64_t* written) {
    
}

XTResult xtFlushBuffers(XTFile* file) {

}
