#include <xt/user.h>
#include <xt/scheduler.h>
#include <stdint.h>
#include <xt/io.h>
#include <xt/kernel.h>

XTResult xtUserWriteFile(
    uint64_t handleId, 
    const void* data, 
    uint64_t size, 
    uint64_t* written
) {
    return xtWriteFile(gSerialDevice, data, 0, size, written);
}

XTResult xtUserTerminateThread(uint64_t handleId, uint64_t code) {

    xtDebugPrint("xtUserTerminateThread!\n");
    XTThread* __currentThread = NULL;
    xtGetCurrentThread(&__currentThread);

    return xtTerminateThread(__currentThread, code);
}

XTResult xtUserReadFile(
    uint64_t handleId, 
    void* data, 
    uint64_t size, 
    uint64_t* read
) {
    return xtReadFile(gSerialDevice, data, 0, size, read);
}

