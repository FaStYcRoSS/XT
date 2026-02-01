#ifndef __XT_IO_H__
#define __XT_IO_H__

#include <xt/result.h>

#include <stdint.h>

typedef struct XTFile XTFile;

typedef XTResult(*PFNXTWRITEFILE)(XTFile* file, const void* data, uint64_t* written);
typedef XTResult(*PFNXTREADFILE)(XTFile* file, void* data, uint64_t* read);
typedef XTResult(*PFNXTDEVICEIOCTL)(XTFile* file, uint64_t code, void* data);

typedef struct XTFileSystemIO {

} XTFileSystemIO;

typedef struct XTFileIO {
    PFNXTWRITEFILE WriteFile;
    PFNXTREADFILE  ReadFile;
    PFNXTDEVICEIOCTL DeviceIO;
} XTFileIO;

typedef struct XTFileSystem {
    const char* Name;
    XTFileSystemIO* IO;
} XTFileSystem;

typedef struct XTMountPoint {
    const char* path;
    XTFileSystem* fs;
    XTFile* device;
    void* data;
} XTMountPoint;

struct XTFile {
    XTMountPoint* mountPoint;
    XTFileIO* IO;
    uint64_t seek;
    void* data;
};

XTResult xtWriteFile(XTFile* file, const void* data, uint64_t* written);
XTResult xtReadFile(XTFile* file, void* data, uint64_t* read);

XTResult xtWriteToBuffer(XTFile* file, const void* data, uint64_t* written);

XTResult xtFlushBuffers(XTFile* file);

extern XTFile* gSerialDevice;

#endif