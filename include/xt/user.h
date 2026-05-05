#ifndef __XT_USER_H__
#define __XT_USER_H__

#include <xt/result.h>
#include <stdint.h>
#include <xt/io.h>

#define XT_CURRENT_PROCESS ((uint64_t)-1)
#define XT_CURRENT_THREAD  ((uint64_t)-2)

#define XT_STDIN           ((uint64_t)0)
#define XT_STDOUT          ((uint64_t)1)
#define XT_STDERR          ((uint64_t)2)

typedef struct XTUserParameters {
    int64_t argc;
    const char** argv;
    char** envp;
    void* imageBase;
} XTUserParameters; 

typedef XTResult(*PFNXTUserMain)(XTUserParameters*);

XTResult xtMain(XTUserParameters* params);

XTResult xtUserWriteFile(
    uint64_t handleId,
    const void* data,
    uint64_t offset,
    uint64_t count,
    uint64_t* written
);

XTResult xtUserExecuteProgram(
    uint64_t processHandle,
    const char** args,
    const char** envp,
    uint64_t* mainThread
);

XTResult xtUserCreateProcess(
    uint64_t parentProcessHandle,
    uint64_t flags,
    uint64_t* newProcessHandle
);

XTResult xtUserDuplicateHandle(
    uint64_t handle,
    uint64_t processHandle,
    uint64_t toSetHandle
);

XTResult xtUserTerminateThread(
    uint64_t handleId,
    uint64_t code
);

XTResult xtUserTerminateProcess(
    uint64_t handleId,
    uint64_t code
);

XTResult xtUserVirtualAlloc(
    uint64_t handleId,
    void* begin,
    uint64_t size,
    uint64_t flags,
    void** out
);

XTResult xtUserVirtualFree(
    uint64_t handleId,
    void* begin,
    uint64_t size
);

XTResult xtUserQueryVirtualMap(
    uint64_t handleId,
    void** begin,
    uint64_t* size,
    uint64_t* flags
);

XTResult xtUserInsertKernelModule(
    const char* filename
);

XTResult xtUserGetCurrentDirectory(
    const char* cwd,
    uint64_t* buffsize
);

XTResult xtUserSetCurrentDirectory(
    const char* cwd
); 

XTResult xtUserOpenDirectory(
    const char* path,
    uint64_t* handle
);

XTResult xtUserReadDirectory(
    uint64_t handle,
    XTFileInfo* info
);

XTResult xtUserCreateThread(
    uint64_t processHandle,
    void(*ThreadFunc)(void*), 
    uint64_t size, 
    void* arg,
    uint8_t state, 
    uint64_t* handle
);

XTResult xtUserCreatePipe(
    uint64_t* writeHandleId,
    uint64_t* readHandleId,
    uint64_t bufferSize
);

XTResult xtUserReadFile(
    uint64_t handleId,
    void* data,
    uint64_t offset,
    uint64_t count,
    uint64_t* read
);

XTResult xtUserCloseHandle(
    uint64_t handle
);


#endif