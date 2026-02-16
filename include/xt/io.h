#ifndef __XT_IO_H__
#define __XT_IO_H__

#include <xt/result.h>

#include <stdint.h>

typedef struct XTFile XTFile;

typedef struct XTMountPoint XTMountPoint;

typedef struct XTFileInfo {
    const char* name;
    uint64_t    size;
    uint64_t    mktime;
    uint64_t    access;
} XTFileInfo;

typedef struct XTDirectory {
    uint64_t pos;
    void*    fsData;
} XTDirectory;
typedef struct XTFileBuffer XTFileBuffer;

typedef XTResult(*PFNXTWRITEBUFFER)(XTFileBuffer* buffer, const void* data, uint64_t* written);
typedef XTResult(*PFNXTREADBUFFER)(XTFileBuffer* buffer, void* data, uint64_t* read);

typedef struct XTFileBuffer {
    XTFile* file;
    void*   ReadData;
    void*   WriteData;
    PFNXTWRITEBUFFER WriteBuffer;
    PFNXTREADBUFFER  ReadBuffer;
} XTFileBuffer;

typedef XTResult(*PFNXTWRITEFILE)(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written);
typedef XTResult(*PFNXTREADFILE)(XTFile* file, void* data, uint64_t offset, uint64_t count, uint64_t* read);
typedef XTResult(*PFNXTDEVICEIOCTL)(XTFile* file, uint64_t code, void* data);
typedef XTResult(*PFNXTMAPFILE)(XTFile* file, uint64_t offset, uint64_t* size, void** out);
typedef XTResult(*PFNXTUNMAPFILE)(XTFile* file, uint64_t offset, void* ptr, uint64_t size);
typedef XTResult(*PFNXTOPENDIRECTORY)(XTMountPoint* mp, const char* name, XTDirectory** out);
typedef XTResult(*PFNXTREADDIRECTORY)(XTDirectory* dir, XTFileInfo* fileInfo);
typedef XTResult(*PFNXTOPENFILE)(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** out);
typedef XTResult(*PFNXTCREATEFILE)(XTMountPoint* mp, const char* name, uint64_t flags);
typedef XTResult(*PFNXTCLOSEFILE)(XTMountPoint* mp, XTFile* file);
typedef XTResult(*PFNXTMOUNT)(XTMountPoint* mp);
typedef XTResult(*PFNXTUNMOUNT)(XTMountPoint* mp);

typedef struct XTFileSystemIO {
    PFNXTOPENDIRECTORY OpenDirectory;
    PFNXTREADDIRECTORY ReadDirectory;
    PFNXTOPENFILE      OpenFile;
    PFNXTCREATEFILE    CreateFile;
    PFNXTMOUNT         Mount;
    PFNXTUNMOUNT       Unmount;
} XTFileSystemIO;

typedef struct XTFileIO {
    PFNXTWRITEFILE   WriteFile;
    PFNXTREADFILE    ReadFile;
    PFNXTDEVICEIOCTL DeviceIO;
    PFNXTMAPFILE     MapFile;
    PFNXTUNMAPFILE   UnmapFile;
    PFNXTCLOSEFILE     CloseFile;
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

struct XTFile {
    XTMountPoint* mountPoint;
    XTFileIO* IO;
    XTFileBuffer* buffer;
    void* data;
};

typedef struct XTFileDescriptor {
    XTFile* file;
    uint64_t seek;
} XTFileDescriptor;

typedef struct XTDescriptor {
    void* desc;
    uint32_t type;
    uint32_t access;
} XTDescriptor;

#define XT_DESCRIPTOR_TYPE_UNDEFINED 0
#define XT_DESCRIPTOR_TYPE_PROCESS   1
#define XT_DESCRIPTOR_TYPE_FILE      2
#define XT_DESCRIPTOR_TYPE_DIRECTORY 3
#define XT_DESCRIPTOR_TYPE_THREAD    4

typedef struct XTDescriptorTable {
    XTDescriptor* descs[512];
} XTDescriptorTable;

#define XT_FILE_MODE_READ           0x01
#define XT_FILE_MODE_WRITE          0x02
#define XT_FILE_MODE_SHARE          0x04
#define XT_FILE_MODE_CREATE         0x08
#define XT_FILE_ATTRIBUTE_DIRECTORY 0x10


XTResult xtWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t size, uint64_t* written);
XTResult xtReadFile(XTFile* file, void* data, uint64_t offset, uint64_t size, uint64_t* read);
XTResult xtOpenFile(const char* path, uint64_t flags, XTFile** out);
XTResult xtCloseFile(XTFile* file);
XTResult xtMount(const char* path, const char* filesystem, XTFile* dev);
XTResult xtUnmount(const char* path);

XTResult xtMapFile(XTFile* file, uint64_t offset, uint64_t* size, void** out);
XTResult xtUnmapFile(XTFile* file, uint64_t offset, void* ptr, uint64_t size);

XTResult xtWriteToBuffer(XTFile* file, const void* data, uint64_t* written);

XTResult xtFlushBuffers(XTFile* file);

XTResult xtSetFileBuffer(XTFile* file, uint64_t flags);

XTResult xtRegisterFileSystem(XTFileSystem* fs);

extern XTFile* gSerialDevice;

extern void* userProgram;

#endif