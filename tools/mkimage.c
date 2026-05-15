
#include <stdio.h>
#include <stdint.h>
#include <efi/efi.h>
#include <xt/io.h>
#include <xt/string.h>
#include <xt/memory.h>
#include <xt/gpt.h>
#define ESP_GUID { 0xC12A7328, 0xF81F, 0x11D2, \
                        {  0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } }

// (Microsoft) Basic Data GUID
#define BASIC_DATA_GUID { 0xEBD0A0A2, 0xB9E5, 0x4433, \
                                { 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } }

#define NULL_GUID {0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}}
XTResult xtPlatformInit();
XTResult xtFileSystemInit();

XTResult xtVFATInit();

XTResult xtFillZeros(XTFile* file) {
    char temp[512];
    xtSetMem(temp, 0, 512);
    XTFileInfo info;
    xtGetFileInfo(file, &info);
    printf("info.FileSize %llu\n", info.FileSize);
    for (uint64_t i = 0; i < info.FileSize; i += 512) {
        xtWriteFile(file, temp, i, 512, NULL);
    }
    return XT_SUCCESS;
}

XTResult xtGetFolderSize(const char* path, uint64_t* size) {
    XTDirectory* dir = NULL;
    xtOpenDirectory(path, &dir);
    XTFileInfo info = { 0 };
    while (xtReadDirectory(dir, &info) == XT_SUCCESS) {
        char* newFileName = 0;

    }
}

XTResult xtRecursiveCopy(const char* source, const char* dest) {
    XTDirectory* dsource = NULL;
    xtOpenDirectory(source, &dsource);
    XTFileInfo info = { 0 };
    while (xtReadDirectory(dsource, &info) == XT_SUCCESS) {
        if (xtStringCmp(info.name, ".", 256) == XT_SUCCESS || xtStringCmp(info.name, "..", 256) == XT_SUCCESS) continue;
        uint64_t destLen = 0;
        uint64_t sourceLen = 0;
        uint64_t nameLen = 0;
        xtGetStringLength(dest, &destLen);
        xtGetStringLength(source, &sourceLen);
        xtGetStringLength(info.name, &nameLen);
        uint64_t newDestLen = destLen + nameLen + 2;
        char* newDestName = NULL;
        xtHeapAlloc(newDestLen, &newDestName);
        xtCopyMem(newDestName, dest, destLen);
        xtCopyMem(newDestName + destLen, "/", 1);
        xtCopyMem(newDestName + destLen + 1, info.name, nameLen);
        newDestName[destLen + 1 + nameLen] = 0;
        uint64_t newSourceLen = sourceLen + nameLen + 2;
        char* newSourceName = NULL;
        xtHeapAlloc(newSourceLen, &newSourceName);
        xtCopyMem(newSourceName, source, sourceLen);
        xtCopyMem(newSourceName + sourceLen, "/", 1);
        xtCopyMem(newSourceName + sourceLen + 1, info.name, nameLen);
        newSourceName[sourceLen + 1 + nameLen] = 0;
        XTFile* newfile = NULL;
        XTResult result = xtOpenFile(newDestName, info.flags, &newfile);
        if (result == XT_NOT_FOUND) {
            xtOpenFile(newDestName, info.flags | XT_FILE_MODE_CREATE, &newfile);
        }
        XTFileInfo newFileInfo = { 0 };
        xtGetFileInfo(newfile, &newFileInfo);
        printf("info.lastWriteTime %llu newFileInfo.lastWriteTime %llu newDestName %s\n", 
            info.lastWriteTime, newFileInfo.lastWriteTime, newDestName);
        // if (newFileInfo.lastWriteTime != 0 && info.lastWriteTime <= newFileInfo.lastWriteTime) {
        //     continue;   // пропускаем, если исходник не новее существующего
        // }
        if (info.flags & XT_FILE_ATTRIBUTE_DIRECTORY) {
            xtRecursiveCopy(newSourceName, newDestName);
        }
        else {
            xtCopyFile(newSourceName, newDestName);
        }
        xtHeapFree(newDestName);
        xtHeapFree(newSourceName);
    }
    return XT_SUCCESS;
}

int main(int argc, char* argv[]) {
    XT_TRY(xtFileSystemInit());
    XT_TRY(xtPlatformInit());
    XTFile* _testFile = NULL;
    XT_TRY(xtOpenFile("/platform/build/xtos1.img", XT_FILE_MODE_CREATE | XT_FILE_MODE_READ | XT_FILE_MODE_WRITE, &_testFile));
    XTFile* efipart = NULL;
    XTFile* syspart = NULL;
    EFI_GUID typeGUID = ESP_GUID;
    EFI_GUID nullGUID = NULL_GUID;
    uint16_t Name[36] = L"EFI SYSTEM PARTITION";
    xtCreatePartitionDevice(
        _testFile, 
        2048, 
        (130*1024*1024)/512 + 2048 - 1,
        Name,
        typeGUID,
        nullGUID,
        &efipart
    );
    EFI_GUID type1GUID = BASIC_DATA_GUID;
    uint16_t Name1[36] = L"Basic Data";
    xtCreatePartitionDevice(
        _testFile,
        (130*1024*1024)/512 + 2048,
        (130*1024*1024)/512 + 8191,
        Name1,
        type1GUID,
        nullGUID,
        &syspart
    );
    XTFile* _files[2] = {
        efipart,
        syspart
    };

    xtSetPartitions(
        _testFile, _files, 2
    );
    
    xtVFATInit();
    XT_TRY(xtMakeFS(efipart, "vfat"));
    XT_TRY(xtMount("/efipart", "vfat", efipart));

    XT_TRY(xtRecursiveCopy("/platform/efipart", "/efipart"));
    XT_TRY(xtCloseFile(_testFile));
    
    return 0;
}

