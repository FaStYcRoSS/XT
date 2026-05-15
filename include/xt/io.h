#ifndef __XT_IO_H__
#define __XT_IO_H__

#include <xt/result.h>
#include <xt/list.h>
#include <stdint.h>
#include <xt/rwlock.h>
#include <xt/event.h>

typedef struct XTWaitable {
    XTList* threads;
} XTWaitable;

typedef struct XTFile XTFile;

typedef struct XTMountPoint XTMountPoint;

typedef struct XTFileInfo {
    char        name[256];
    uint64_t    FileSize;
    uint64_t    PhysicalSize;
    uint64_t    createdTime;
    uint64_t    lastAccessTime;
    uint64_t    lastWriteTime;
    uint64_t    flags;
    uint64_t    userOwnerId;
} XTFileInfo;

typedef struct XTDirectory XTDirectory;
typedef struct XTFileBuffer XTFileBuffer;

#define XT_PATH_NODE_TYPE_FILE      0
#define XT_PATH_NODE_TYPE_DIRECTORY 1


typedef struct XTPathNode {
    XTMountPoint* mp;
    const char* name;
    XTList*     nodes;
} XTPathNode;

struct XTDirectory {
    XTPathNode* node;
    void* data;
    uint64_t pos;
};

typedef XTResult(*PFNXTWRITEFILE)(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written);
typedef XTResult(*PFNXTREADFILE)(XTFile* file, void* data, uint64_t offset, uint64_t count, uint64_t* read);
typedef XTResult(*PFNXTDEVICEIOCTL)(XTFile* file, uint64_t code, void* data);
typedef XTResult(*PFNXTMAPFILE)(XTFile* file, uint64_t offset, uint64_t* size, void** out);
typedef XTResult(*PFNXTUNMAPFILE)(XTFile* file, uint64_t offset, void* ptr, uint64_t size);
typedef XTResult(*PFNXTOPENDIRECTORY)(XTMountPoint* mp, const char* name, XTDirectory* out);
typedef XTResult(*PFNXTREADDIRECTORY)(XTDirectory* dir, XTFileInfo* fileInfo);
typedef XTResult(*PFNXTDELETEFILE)(XTMountPoint* mp, const char* name);
typedef XTResult(*PFNXTOPENFILE)(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** out);
typedef XTResult(*PFNXTCREATEFILE)(XTMountPoint* mp, const char* name, uint64_t flags);
typedef XTResult(*PFNXTCLOSEFILE)(XTMountPoint* mp, XTFile* file);
typedef XTResult(*PFNXTMOUNT)(XTMountPoint* mp);
typedef XTResult(*PFNXTUNMOUNT)(XTMountPoint* mp);
typedef XTResult(*PFNXTGETFILEINFO)(XTFile* file, XTFileInfo* info);
typedef XTResult(*PFNXTSETFILEINFO)(XTFile* file, XTFileInfo* info);
typedef XTResult(*PFNXTMAKEFILESYSTEM)(XTFile* file);

typedef struct XTFileSystemIO {
    PFNXTOPENDIRECTORY OpenDirectory;
    PFNXTREADDIRECTORY ReadDirectory;
    PFNXTOPENFILE      OpenFile;
    PFNXTCREATEFILE    CreateFile;
    PFNXTMOUNT         Mount;
    PFNXTUNMOUNT       Unmount;
    PFNXTMAKEFILESYSTEM MakeFS;
    PFNXTDELETEFILE    DeleteFile;
} XTFileSystemIO;

typedef struct XTFileIO {
    PFNXTWRITEFILE   WriteFile;
    PFNXTREADFILE    ReadFile;
    PFNXTDEVICEIOCTL DeviceIO;
    PFNXTMAPFILE     MapFile;
    PFNXTUNMAPFILE   UnmapFile;
    PFNXTCLOSEFILE   CloseFile;
    PFNXTGETFILEINFO GetFileInfo;
    PFNXTSETFILEINFO SetFileInfo;
} XTFileIO;

typedef struct XTFileSystem {
    const char* Name;
    XTFileSystemIO* IO;
} XTFileSystem;

typedef struct XTMountPoint {
    XTFileSystem* fs;
    XTFile* device;
    void* data;
} XTMountPoint;

typedef struct XTThread XTThread;

struct XTFile {
    XTMountPoint* mountPoint;
    XTFileIO* IO;
    void* data;
    uint64_t flags;
    XTRWLock lock;
    XTEvent* event;
};

typedef struct XTDescriptor {
    void* desc;
    uint32_t type;
    uint32_t access;
} XTDescriptor;

#define XT_FILE_MODE_READ           0x01
#define XT_FILE_MODE_WRITE          0x02
#define XT_FILE_MODE_CREATE         0x04
#define XT_FILE_ATTRIBUTE_DIRECTORY 0x08
#define XT_FILE_SHARE_DELETE        0x10
#define XT_FILE_SHARE_READ          0x20
#define XT_FILE_SHARE_WRITE         0x40
#define XT_FILE_MODE_NONBLOCK       0x80
#define XT_LOCK_READ                0x01
#define XT_LOCK_WRITE               0x02

XTResult xtWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t size, uint64_t* written);
XTResult xtReadFile(XTFile* file, void* data, uint64_t offset, uint64_t size, uint64_t* read);
XTResult xtOpenFile(const char* path, uint64_t flags, XTFile** out);
XTResult xtCloseFile(XTFile* file);
XTResult xtMount(const char* path, const char* filesystem, XTFile* dev);
XTResult xtUnmount(const char* path);
XTResult xtReadDirectory(XTDirectory* dir, XTFileInfo* info);
XTResult xtOpenDirectory(const char* path, XTDirectory** dir);
XTResult xtGetFileInfo(XTFile* file, XTFileInfo* info);
XTResult xtSetFileInfo(XTFile* file, XTFileInfo* info);
XTResult xtCopyFile(const char* source, const char* dest);
XTResult xtMoveFile(const char* path1, const char* path2);
XTResult xtDeleteFile(const char* path);
XTResult xtLockFile(XTFile* file, uint64_t lockType);
XTResult xtUnlockFile(XTFile* file, uint64_t lockType);

XTResult xtMapFile(XTFile* file, uint64_t offset, uint64_t* size, void** out);
XTResult xtUnmapFile(XTFile* file, uint64_t offset, void* ptr, uint64_t size);

XTResult xtWriteToBuffer(XTFile* file, const void* data, uint64_t* written);

XTResult xtFlushBuffers(XTFile* file);

XTResult xtRegisterFileSystem(XTFileSystem* fs);
XTResult xtMakeFS(XTFile* file, const char* filesystem);

XTResult xtRegisterDevice(const char* name, XTFile* file);
XTResult xtUnregisterDevice(const char* name);

XTResult xtCreatePipe(
    XTFile** write,
    XTFile** read,
    uint64_t sizeOfBuffer
);

XTResult xtCreateNamedPipe(
    const char* path,
    uint64_t bufferSize
);

extern XTFile* gSerialDevice;

extern void* userProgram;

#endif