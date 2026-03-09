#ifndef __XT_USER_H__
#define __XT_USER_H__

#include <xt/result.h>
#include <stdint.h>

#define XT_CURRENT_PROCESS ((uint64_t)-1)
#define XT_CURRENT_THREAD  ((uint64_t)-2)

#define XT_STDIN           ((uint64_t)0)
#define XT_STDOUT          ((uint64_t)1)
#define XT_STDERR          ((uint64_t)2)

typedef XTResult(*PFNXTMAIN)(int argc, char** argv, char** envp);

int64_t main(int argc, char* argv[], char* envp[]);

typedef struct XTUserModuleInstance {
    const char* name;
    void* base;
    struct XTUserModuleInstance* next;
} XTUserModuleInstance;

typedef struct XTProcessEnvironmentBlock {
    const char** args;
    const char** envp;
    XTUserModuleInstance* modules;
} XTProcessEnvironmentBlock;

typedef struct XTThreadEnvironmentBlock {
    XTProcessEnvironmentBlock* peb;
    void* tls;
} XTThreadEnvironmentBlock;

XTResult xtUserWriteFile(
    uint64_t handleId,
    const void* data,
    uint64_t count,
    uint64_t* written
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

XTResult xtUserLoadModule(
    const char* filename,
    void** base
);

XTResult xtUserFreeModule(
    const char* filename,
    void* base
);

XTResult xtUserLoadKernelModule(
    const char* filename
);

XTResult xtUserGetCurrentDirectory(
    const char* cwd,
    uint64_t* buffsize
);

XTResult xtUserSetCurrentDirectory(
    const char* cwd
); 

XTResult xtUserReadFile(
    uint64_t handleId,
    void* data,
    uint64_t count,
    uint64_t* read
);


#endif