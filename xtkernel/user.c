#include <xt/user.h>
#include <xt/scheduler.h>
#include <stdint.h>
#include <xt/io.h>
#include <xt/kernel.h>

XTResult xtUserWriteFile(
    uint64_t handleId, 
    const void* data, 
    uint64_t offset,
    uint64_t size, 
    uint64_t* written
) {
    XTProcess* process = NULL;
    xtGetCurrentProcess(&process);
    XTDescriptor* desc = NULL;
    XTResult result = xtGetHandle(process, handleId, &desc);
    if (result == XT_NOT_FOUND) {
        result = XT_INVALID_PARAMETER;
    }
    if (desc->type != XT_DESCRIPTOR_TYPE_FILE) result = XT_INVALID_PARAMETER;
    if (!(desc->access & XT_FILE_MODE_WRITE)) result = XT_ACCESS_DENIED;
    if (XT_IS_ERROR(result)) {
        XT_ASSERT(result);
    }
    result = xtWriteFile(desc->desc, data, 0, size, written);
    xtDebugPrint("Result is %s\n", xtResultToStr(result));
}

XTResult xtUserTerminateThread(uint64_t handleId, uint64_t code) {

    XTThread* thread = NULL;
    if (handleId == XT_CURRENT_THREAD) {
        xtGetCurrentThread(&thread);
    }
    else {
        XTProcess* currentProcess = NULL;
        xtGetCurrentProcess(&currentProcess);
        XTDescriptor* desc = NULL;
        XTResult result = xtGetHandle(currentProcess, handleId, &desc);
        if (XT_IS_ERROR(result)) return result;
        thread = desc->desc;
    }
    return xtTerminateThread(thread, code);
}

XTResult xtUserReadFile(
    uint64_t handleId, 
    void* data, 
    uint64_t offset,
    uint64_t size, 
    uint64_t* read
) {
    XTProcess* process = NULL;
    xtGetCurrentProcess(&process);
    XTDescriptor* desc = NULL;
    XTResult result = xtGetHandle(process, handleId, &desc);
    if (result == XT_NOT_FOUND) {
        return XT_INVALID_PARAMETER;
    }
    if (desc->type != XT_DESCRIPTOR_TYPE_FILE) return XT_INVALID_PARAMETER;
    if (!(desc->access & XT_FILE_MODE_READ)) return XT_ACCESS_DENIED;
    return xtReadFile(desc->desc, data, 0, size, read);
}

