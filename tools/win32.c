#include <xt/result.h>
#include <xt/io.h>
#include <xt/memory.h>
#include <xt/random.h>
#include <xt/string.h>
#include <xt/encodings.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#undef CreateFile

XTResult xtDebugPrint(const char* format, ...) {
    va_list va;
    va_start(va, format);
    vprintf(format, va);
    va_end(va);
}

XTResult xtHeapAlloc(uint64_t size, void** data) {
    *data = malloc(size);
    return XT_SUCCESS;
}

XTResult xtHeapFree(void* data) {
    free(data);
    return XT_SUCCESS;
}

XTResult xtGetRandomU64(uint64_t* random) {
    static int isFirstRun = 0;
    if (!isFirstRun) {
        srand(time(NULL));
        isFirstRun = 1;
    }
    uint32_t randomLow = rand();
    uint32_t randomHigh = rand();
    *random = randomLow | ((uint64_t)randomHigh << 32);
    return XT_SUCCESS;
}

#define TICKS_PER_SECOND 10000000ULL
#define EPOCH_DIFFERENCE 11644473600ULL

uint64_t nt2unixTime(FILETIME ft) {
    return (*(uint64_t*)&ft / TICKS_PER_SECOND) - EPOCH_DIFFERENCE;
} 

XTResult xtGetTime(uint64_t* unixtime) {
    *unixtime = _time64(NULL);
    return XT_SUCCESS;
}

XTResult xtSetTime(uint64_t unixtime) {
    return XT_NOT_IMPLEMENTED;
}

XTResult win32GetFileInfo(XTFile* file, XTFileInfo* info) {
    if (!GetFileSizeEx(file->data, &info->FileSize)) {
        xtDebugPrint("WHAT?");
    }
    info->PhysicalSize = ((info->FileSize >> 12) + 1) << 12;
    BY_HANDLE_FILE_INFORMATION byHandle = { 0 };
    GetFileInformationByHandle(file->data, &byHandle);
    info->createdTime = nt2unixTime(byHandle.ftCreationTime);
    info->lastAccessTime = nt2unixTime(byHandle.ftLastAccessTime);
    info->lastWriteTime = nt2unixTime(byHandle.ftLastWriteTime);
    WCHAR wname[MAX_PATH];
    DWORD dwRet = GetFinalPathNameByHandleW(file->data, wname, MAX_PATH, VOLUME_NAME_DOS);
    xtUTF16toUTF8(wname, info->name);
    info->flags = (byHandle.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
              ? XT_FILE_ATTRIBUTE_DIRECTORY : 0;
    info->flags |= XT_FILE_MODE_READ | XT_FILE_MODE_WRITE;
    return XT_SUCCESS;
}

XTResult win32Read(XTFile* file, void* data, uint64_t offset, uint64_t count, uint64_t* written) {
    HANDLE hFile = file->data;
    DWORD read = 0;
    SetFilePointer(hFile, offset & 0xffffffff, offset >> 32, FILE_BEGIN);
    BOOL _bool = ReadFile(hFile, data, count, &read, NULL);
    if (!_bool) return XT_UNKNOWN_ERROR;
    if (read == 0) {
        return XT_END_OF_FILE;
    }
    *written = read;
    return XT_SUCCESS;
}

XTResult win32Write(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written) {
    HANDLE hFile = file->data;
    DWORD read = 0;
    SetFilePointer(hFile, offset & 0xffffffff, offset >> 32, FILE_BEGIN);
    BOOL _bool = WriteFile(hFile, data, count, &read, NULL);
    if (!_bool) return XT_UNKNOWN_ERROR;
    if (read == 0) {
        return XT_END_OF_FILE;
    }
    *written = read;
    return XT_SUCCESS;

}



XTResult win32Close(XTMountPoint* mp, XTFile* file) {
    CloseHandle(file->data);
    return XT_SUCCESS;
}

XTFileIO win32IO = {
    .ReadFile = win32Read,
    .WriteFile = win32Write,
    .CloseFile = win32Close,
    .GetFileInfo = win32GetFileInfo
};

XTResult win32OpenFile(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** out);
XTResult win32CreateFile(XTMountPoint* mp, const char* name, uint64_t flags) {
    WCHAR wname[256];
    xtUTF8toUTF16(name, wname);
    HANDLE hFile = CreateFileW(
        wname,
        GENERIC_ALL,
        0,
        NULL,
        CREATE_NEW, 0, NULL
    );
    if (hFile == NULL) {
        return XT_ACCESS_DENIED;
    }
    CloseHandle(hFile);
    return XT_SUCCESS;
}

typedef struct win32DirEntry {
    HANDLE hFile;
    WIN32_FIND_DATAW findData;
    int firstCall;
} win32DirEntry;

XTResult win32OpenDirectory(XTMountPoint* mp, const char* path, XTDirectory* out) {
    WCHAR wname[260];
    xtUTF8toUTF16(path, wname);
    WCHAR* new_wname = wcscat(wname, L"/*");
    printf("wname %ls\n", new_wname);
    win32DirEntry* dirEntry = NULL;
    xtHeapAlloc(sizeof(win32DirEntry), &dirEntry);
    dirEntry->hFile = FindFirstFileW(new_wname, &dirEntry->findData);
    dirEntry->firstCall = 1;
    out->data = dirEntry;
    return XT_SUCCESS;
}

XTResult win32ReadDirectory(XTDirectory* dir, XTFileInfo* info) {
    win32DirEntry* dirEntry = dir->data;
    if (dirEntry->hFile == NULL) return XT_END_OF_FILE;
    if (!dirEntry->firstCall) {
        if (!FindNextFileW(dirEntry->hFile, &dirEntry->findData)) return XT_END_OF_FILE;
    }
    else {
        dirEntry->firstCall = 0;
    }
    xtUTF16toUTF8(dirEntry->findData.cFileName, info->name);
    info->createdTime = nt2unixTime(dirEntry->findData.ftCreationTime);
    info->lastAccessTime = nt2unixTime(dirEntry->findData.ftLastAccessTime);
    info->lastWriteTime = nt2unixTime(dirEntry->findData.ftLastWriteTime);
    info->FileSize = ((uint64_t)dirEntry->findData.nFileSizeHigh << 32) | (dirEntry->findData.nFileSizeLow);
    info->PhysicalSize = ((info->FileSize >> 12) + 1) << 12;
    info->flags = (dirEntry->findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
              ? XT_FILE_ATTRIBUTE_DIRECTORY : 0;
    return XT_SUCCESS;
}

XTResult win32Mount(XTMountPoint* mp) {
    return XT_SUCCESS;
}

XTFileSystemIO win32fsIO = {
    .OpenFile = win32OpenFile,
    .CreateFile = win32CreateFile,
    .Mount = win32Mount,
    .OpenDirectory = win32OpenDirectory,
    .ReadDirectory = win32ReadDirectory
};

XTResult win32OpenFile(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** out) {
    DWORD dwCreationDispotition = 0;
    DWORD dwDesiredAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    if (flags & XT_FILE_MODE_READ) {
        dwDesiredAccess |= GENERIC_READ;
    }
    if (flags & XT_FILE_MODE_WRITE) {
        dwDesiredAccess |= GENERIC_WRITE;
    }
    if (flags & XT_FILE_SHARE_DELETE) {
        dwShareMode |= FILE_SHARE_DELETE;
    }
    if (flags & XT_FILE_SHARE_READ) {
        dwShareMode |= FILE_SHARE_READ;
    }
    if (flags & XT_FILE_SHARE_WRITE) {
        dwShareMode |= FILE_SHARE_WRITE;
    }
    if (flags & XT_FILE_MODE_CREATE) {
        dwCreationDispotition = CREATE_NEW;
    }
    else {
        dwCreationDispotition = OPEN_ALWAYS;
    }
    WCHAR wname[256];
    xtUTF8toUTF16(name, wname);
    HANDLE hFile = CreateFileW(
        wname,
        dwDesiredAccess,
        dwShareMode,
        NULL,
        dwCreationDispotition, 0, NULL
    );
    if (hFile == NULL) {
        return XT_ACCESS_DENIED;
    }
    XTFile* file = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTFile), &file));
    file->data = hFile;
    file->IO = &win32IO;
    file->mountPoint = mp;
    *out = file;
    return XT_SUCCESS;
}

XTFileSystem win32fs = {
    .Name = "win32fs",
    .IO = &win32fsIO
};



XTResult xtPlatformInit() {
    XT_TRY(xtRegisterFileSystem(&win32fs));
    XT_TRY(xtMount("/platform", "win32fs", NULL));
    return XT_SUCCESS;
}