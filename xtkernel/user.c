#include <xt/user.h>
#include <xt/scheduler.h>
#include <stdint.h>
#include <xt/io.h>
#include <xt/kernel.h>
#include <xt/memory.h>

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
    void* newData = NULL;
    xtHeapAlloc(size, &newData);
    result = xtCopyFromUser(newData, data, size);

    if (result == XT_ACCESS_VIOLATION) {
        xtHeapFree(newData);
        return result;
    }
    result = xtWriteFile(desc->desc, newData, offset, size, written);
    xtHeapFree(newData);
    return result;
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
    void* newData = NULL;
    xtHeapAlloc(size, &newData);
    result = xtReadFile(desc->desc, newData, offset, size, read);
    XTResult copyResult = xtCopyToUser(data, newData, read);
    xtHeapFree(newData);
    if (copyResult == XT_ACCESS_VIOLATION) {
        return copyResult;
    }
    return result;
}

