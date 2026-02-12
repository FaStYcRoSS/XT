#include <xt/io.h>
#include <xt/list.h>
#include <xt/memory.h>

XTResult xtWriteFile(XTFile* file, const void* data, uint64_t* written) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);
    XT_CHECK_ARG_IS_NULL(written);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;

    if (file->IO->WriteFile == NULL) return XT_NOT_IMPLEMENTED;
    
    return file->IO->WriteFile(file, data, written);

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

XTResult xtOpenFile(const char* path, uint64_t flags, XTFile** out) {
    if (root == NULL) return XT_NOT_FOUND;
    return root->mp->fs->IO->OpenFile(root->mp, path+1, flags, out);
}

XTResult xtCloseFile(XTFile* file) {
    return file->IO->CloseFile(file->mountPoint, file);
}

XTResult xtMount(const char* path, const char* filesystem, XTFile* dev) {
    
}

XTResult xtUnmount(const char* path) {

}

XTResult xtReadFile(XTFile* file, void* data, uint64_t* read) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);
    XT_CHECK_ARG_IS_NULL(read);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;

    if (file->IO->ReadFile == NULL) return XT_NOT_IMPLEMENTED;
    
    return file->IO->ReadFile(file, data, read);
}

XTResult xtWriteToBuffer(XTFile* file, const void* data, uint64_t* written) {
    
}

XTResult xtFlushBuffers(XTFile* file);
