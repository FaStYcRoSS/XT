#ifndef __XT_LINKER_H__
#define __XT_LINKER_H__

#include <xt/result.h>
#include <xt/user.h>
#include <xt/scheduler.h>
#include <stdint.h>

XTResult xtLoadModule(
    XTProcess* process,
    const char* filename, 
    void* virtualBase, 
    uint64_t flags,
    PFNXTMAIN* out
);

#endif