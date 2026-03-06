#ifndef __XT_LINKER_H__
#define __XT_LINKER_H__

#include <xt/result.h>
#include <xt/user.h>
#include <xt/scheduler.h>
#include <stdint.h>

typedef XTResult(*PFNXTDRIVERMAIN)(int reason);
typedef XTResult(*PFNXTLIBRARYMAIN)(int reason);

#define XT_LIBRARY_UNKNOWN 0
#define XT_LIBRARY_ATTACH  1
#define XT_LIBRARY_DETACH  2

XTResult xtLoadUserModule(
    XTProcess* process,
    const char* filename, 
    void* virtualBase, 
    uint64_t flags,
    PFNXTMAIN* out
);

XTResult xtLoadKernelModule(
    const char* filename,
    PFNXTDRIVERMAIN* out
);

XTResult xtExecuteProgram(
    XTProcess* process,
    const char** args,
    const char** evnp
);

#endif