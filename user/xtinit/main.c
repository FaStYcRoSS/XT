#include <xt/user.h>
#include <stdint.h>

#include "../../external/printf.h"

void ___chkstk_ms() {

}

void _putchar(char _ch) {

}

#define XT_ASSERT(x) do { \
    XTResult __result = x; \
    if (XT_IS_ERROR(__result)) {\
        char buff[512];\
        int n = snprintf(buff, 512, "%d:%s:%s %s %llx\n", __LINE__, __FILE__, __FUNCTION__, #x, __result); \
        xtUserWriteFile(XT_STDOUT, buff, 0, n, NULL); \
        xtUserTerminateThread(XT_CURRENT_THREAD, __result); \
    }\
} while(0)

XTResult xtMain(XTUserParameters* params) {
    
    uint64_t writePipe = 0;
    uint64_t readPipe = 0;
    uint64_t process = 0;
    uint64_t mainThread = 0;
    XT_ASSERT(xtUserCreateProcess(
        XT_CURRENT_PROCESS,
        0,
        &process
    ));
    XT_ASSERT(
        xtUserCreatePipe(
            &writePipe,
            &readPipe,
            0
        )
    );
    XT_ASSERT(xtUserDuplicateHandle(
        writePipe,
        process,
        XT_STDOUT
    ));
    const char* args[] = {
        "/initrd/test.xte",
        NULL
    };
    XT_ASSERT(xtUserExecuteProgram(
        process,
        args,
        NULL,
        &mainThread
    ));
    char from_pipe[512];
    xtUserReadFile(readPipe, from_pipe, 0, 512, NULL);
    char buff[512];
    int n = snprintf(buff, 512, "pipe: %s\n", from_pipe);
    return xtUserWriteFile(XT_STDOUT, buff, 0, n, NULL);
}
