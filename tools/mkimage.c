
#include <stdio.h>
#include <stdint.h>
#include <efi/efi.h>
#include <xt/io.h>
#include <xt/string.h>
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

int main(int argc, char* argv[]) {
    XT_TRY(xtFileSystemInit());
    XT_TRY(xtPlatformInit());
    XTFile* _testFile = NULL;
    XT_TRY(xtOpenFile("/platform/build/xtos1.img", XT_FILE_MODE_CREATE | XT_FILE_MODE_READ | XT_FILE_MODE_WRITE, &_testFile));
    xtFillZeros(_testFile);
    XTFile* efipart = NULL;
    XTFile* syspart = NULL;
    EFI_GUID typeGUID = ESP_GUID;
    EFI_GUID nullGUID = NULL_GUID;
    uint16_t Name[36] = L"EFI SYSTEM PARTITION";
    xtCreatePartitionDevice(
        _testFile,
        2048,
        4095,
        Name,
        typeGUID,
        nullGUID,
        &efipart
    );
    EFI_GUID type1GUID = BASIC_DATA_GUID;
    uint16_t Name1[36] = L"Basic Data";
    xtCreatePartitionDevice(
        _testFile,
        4096,
        8191,
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
    XT_TRY(xtMount("/efipart", "vfat", efipart));
    XT_TRY(xtMakeFS("/efipart"));
    XTFile* out = NULL;
    xtOpenFile("/efipart/test.txt", XT_FILE_MODE_CREATE | XT_FILE_MODE_WRITE | XT_FILE_MODE_READ, &out);
    xtWriteFile(out, "hello, VFAT!", 0, 12, NULL);
    xtCloseFile(out);
    XT_TRY(xtCloseFile(_testFile));
    
    return 0;
}

