#include <xt/io.h>
#include <xt/kernel.h>
#include <xt/string.h>
#include <xt/memory.h>

typedef struct Tar {
    char filename[100]; //100
    char mode[8]; //108
    char uid[8]; //116
    char gid[8]; //124
    char size[12]; //136
    char mtime[12]; //148
    char chksum[8]; //156
    char typeflag[1]; //157
    char link[100]; //257
    char magic[6]; //263
    char version[2]; //265
    char userOwner[32]; //297
    char groupOwner[32]; //329
    char devMajor[8]; //337
    char devMinor[8]; //345
    char prefix[167]; //512
} Tar;

uint64_t oct2bin(unsigned char *str, int size) {
    uint64_t n = 0;
    unsigned char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}
XTResult xtStringCmp(const char* left, const char* right, uint64_t max);

void* userProgram = NULL;

typedef struct TarFileData {
    uint64_t fileSize;
    const char* name;
    uint64_t ptrToData;
    uint64_t mktime;
    uint64_t access;
    uint64_t attr;
} TarFileData;

XTResult tarReadFile(XTFile* file, void* data, uint64_t offset, uint64_t count, uint64_t* read) {

    TarFileData* fileData = file->data;
    XTFile* dev = file->mountPoint->device;
    uint64_t seek = fileData->ptrToData + offset;
    return xtReadFile(dev, data, seek, count, read);

}

XTResult tarMapFile(XTFile* file, uint64_t offset, uint64_t* size, void** out) {

    TarFileData* fileData = file->data;
    XTFile* dev = file->mountPoint->device;
    *size = fileData->fileSize;
    XT_TRY(xtMapFile(dev, offset + fileData->ptrToData, size, out));
    return XT_SUCCESS;
}

XTResult tarUnmapFile(XTFile* file, uint64_t offset, void* ptr, uint64_t size) {
    TarFileData* fileData = file->data;
    if (size == 0) {
        size = fileData->fileSize;
    }
    return xtUnmapFile(file, offset, ptr, size);
}

XTFileIO fileIO = {
    .ReadFile = tarReadFile,
    .MapFile = tarMapFile,
    .UnmapFile = tarUnmapFile
};

XTResult tarMount(XTMountPoint* mp) {
    return XT_SUCCESS;
}

XTResult tarOpenFile(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** out) {
    uint64_t seek = 0;
    while (1) {
        Tar header = { 0 };
        uint64_t read = 0;
        XTResult result = xtReadFile(mp->device, &header, seek, sizeof(Tar), &read);
        if (result == XT_END_OF_FILE) {
            return XT_NOT_FOUND;
        }
        if (xtStringCmp(header.filename, name, 100) == XT_SUCCESS) {
            XTFile* file = NULL;
            XT_TRY(xtHeapAlloc(sizeof(XTFile), &file));
            TarFileData* fileData = NULL;
            XT_TRY(xtHeapAlloc(sizeof(TarFileData), &fileData));
            xtDuplicateString(header.filename, &fileData->name);
            fileData->ptrToData = seek+sizeof(Tar);
            fileData->fileSize = oct2bin(header.size, 12);
            fileData->mktime = oct2bin(header.mtime, 12);
            int type = oct2bin(header.typeflag, 1);
            if (type == 5) {
                fileData->attr = XT_FILE_ATTRIBUTE_DIRECTORY;
            }
            fileData->access = flags;
            file->data = fileData;
            file->IO = &fileIO;
            file->mountPoint = mp;
            file->buffer = NULL;
            *out = file;
            break;
        }
        seek += sizeof(Tar);
    }

    return XT_SUCCESS;
}

XTFileSystemIO fsIO = {
    .Mount = tarMount,
    .OpenFile = tarOpenFile
};

XTFileSystem tarfs = {
    .Name = "tarfs",
    .IO = &fsIO
};

typedef struct XTInitrdData {
    void* ptr;
    uint64_t size;
} XTInitrdData;

XTInitrdData initrdData = { 0 };

XTResult initrdReadFile(XTFile* file, const void* data, uint64_t offset, uint64_t size, uint64_t* read) {
    XTInitrdData* _initrdData = file->data;
    if (offset >= _initrdData->size) {
        return XT_END_OF_FILE;
    }
    xtCopyMem(data, (void*)(offset + (char*)_initrdData->ptr), size);
    *read = size;
    return XT_SUCCESS;
}

XTResult initrdMapFile(XTFile* file, uint64_t offset, uint64_t* size, void** out) {
    XTInitrdData* _initrdData = file->data;
    *out = (char*)_initrdData->ptr + offset;
    return XT_SUCCESS;
}

XTFileIO initrdIO = {
    .ReadFile = initrdReadFile,
    .MapFile = initrdMapFile
};

XTFile initrd = { 0 };



XTResult xtRamDiskInit() {

    XT_TRY(xtRegisterFileSystem(&tarfs));


    initrdData.ptr = gKernelBootInfo->initrd;
    initrdData.size = gKernelBootInfo->initrdSize;
    initrd.data = &initrdData;
    initrd.IO = &initrdIO;

    XT_TRY(xtMount("/", "tarfs", &initrd));
    return XT_SUCCESS;

}