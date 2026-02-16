#ifndef __XT_USER_H__
#define __XT_USER_H__

#include <xt/result.h>
#include <stdint.h>

#define XT_CURRENT_PROCESS ((uint64_t)-1)
#define XT_CURRENT_THREAD  ((uint64_t)-2)

#define XT_STDIN           ((uint64_t)0)
#define XT_STDOUT          ((uint64_t)1)
#define XT_STDERR          ((uint64_t)2)

XTResult xtMain(const char** args);

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

XTResult xtUserReadFile(
    uint64_t handleId,
    void* data,
    uint64_t count,
    uint64_t* read
);


#endif