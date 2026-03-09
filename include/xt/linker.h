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

#define XT_DRIVER_WAS_LOADED 0
#define XT_DRIVER_WAS_INITED 1

XTResult xtFindModuleByPtr(
    void* ptr,
    const char** filename
);  

XTResult xtLoadKernelModule(
    const char* filename,
    void** base
);

XTResult xtExecuteProgram(
    XTProcess* process,
    const char** args,
    const char** evnp
);

#endif