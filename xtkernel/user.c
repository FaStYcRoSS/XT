#include <xt/user.h>
#include <xt/scheduler.h>
#include <stdint.h>
#include <xt/io.h>
#include <xt/kernel.h>
#include <xt/memory.h>
#include <xt/linker.h>

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
        return XT_INVALID_PARAMETER;
    }
    if (desc->type != XT_DESCRIPTOR_TYPE_FILE) return XT_INVALID_PARAMETER;
    if (!(desc->access & XT_FILE_MODE_WRITE)) return XT_ACCESS_DENIED;
    if (XT_IS_ERROR(result)) {
        return result;
    }
    void* newData = NULL;
    xtHeapAlloc(size, &newData);
    result = xtCopyFromUser(newData, data, size);
    if (result == XT_ACCESS_VIOLATION) {
        xtHeapFree(newData);
        return result;
    }
    uint64_t temp_written = 0;
    result = xtWriteFile(desc->desc, newData, offset, size, &temp_written);
    xtCopyToUser(written, &temp_written, 8);
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
    XT_TRY(xtHeapAlloc(size, &newData));
    uint64_t tempRead = 0;
    result = xtReadFile(desc->desc, newData, offset, size, &tempRead);
    XTResult copyResult = xtCopyToUser(data, newData, tempRead);
    if (copyResult == XT_ACCESS_VIOLATION) {
        return copyResult;
    }
    xtHeapFree(newData);
    xtCopyToUser(read, &tempRead, 8);
    return result;
}

XTResult xtFindFreeHandle(XTProcess* process, uint64_t* freeHandle);

XTResult xtUserDuplicateHandle(
    uint64_t handle,
    uint64_t processHandle,
    uint64_t toSetHandle
) {
    XTDescriptor* desc = NULL;
    XTProcess* currentProcess = NULL;
    XTProcess* anotherProcess = NULL;
    xtGetCurrentProcess(&currentProcess);
    if (processHandle == XT_CURRENT_PROCESS) {
        anotherProcess = currentProcess;
    }
    else {
        XTDescriptor* anotherProcessDesc = NULL;
        XT_TRY(xtGetHandle(currentProcess, processHandle, &anotherProcessDesc));
        if (anotherProcessDesc->type != XT_DESCRIPTOR_TYPE_PROCESS) {
            xtDebugPrint("anotherProcessDesc->type %u\n", anotherProcessDesc->type);
            return XT_INVALID_PARAMETER;
        }
        anotherProcess = anotherProcessDesc->desc;
    }
    XT_TRY(xtGetHandle(currentProcess, handle, &desc));
    XT_TRY(xtDuplicateHandle(anotherProcess, toSetHandle, desc->access, desc->type, desc->desc));
    return XT_SUCCESS;
}

XTResult xtUserCreatePipe(
    uint64_t* writeHandle,
    uint64_t* readHandle,
    uint64_t bufferSize
) {
    XTProcess* process = NULL;
    xtGetCurrentProcess(&process);
    XTFile* writePipe = NULL;
    XTFile* readPipe = NULL;
    XT_TRY(xtCreatePipe(
        &writePipe,
        &readPipe,
        bufferSize
    ));
    uint64_t writeHandleId = 0;
    uint64_t readHandleId = 0;
    xtFindFreeHandle(process, &writeHandleId);
    XT_TRY(xtDuplicateHandle(
        process,
        writeHandleId, XT_FILE_MODE_WRITE, XT_DESCRIPTOR_TYPE_FILE, writePipe
    ));
    xtCopyToUser(writeHandle, &writeHandleId, 8);
    xtFindFreeHandle(process, &readHandleId);
    XT_TRY(xtDuplicateHandle(
        process,
        readHandleId, XT_FILE_MODE_READ, XT_DESCRIPTOR_TYPE_FILE, readPipe
    ));
    xtCopyToUser(readHandle, &readHandleId, 8);
    return XT_SUCCESS;
}

XTResult xtGetArgsCount(const char** args, uint64_t* argc);

XTResult xtUserCreateProcess(
    uint64_t parentProcessHandle,
    uint64_t flags,
    uint64_t* newProcessHandle
) {
    XTProcess* process = NULL;
    XTProcess* currentProcess = NULL;
    xtGetCurrentProcess(&currentProcess);
    if (parentProcessHandle == XT_CURRENT_PROCESS) {
        process = currentProcess;
    }
    else {
        XTDescriptor* desc = NULL;
        XT_TRY(xtGetHandle(currentProcess, parentProcessHandle, &desc));
        if (desc->type != XT_DESCRIPTOR_TYPE_PROCESS) return XT_INVALID_PARAMETER;
        process = desc->desc;
    }
    XTProcess* newProcess = NULL;
    XT_TRY(xtCreateProcess(
        process,
        flags,
        &newProcess
    ));
    uint64_t newProcessHandleId = 0;
    XT_TRY(xtFindFreeHandle(currentProcess, &newProcessHandleId));
    XT_TRY(xtDuplicateHandle(
        currentProcess, newProcessHandleId, 0, XT_DESCRIPTOR_TYPE_PROCESS, newProcess
    ));
    XTResult copyResult = xtCopyToUser(newProcessHandle, &newProcessHandleId, 8);
    XT_TRY(copyResult);
    return XT_SUCCESS;
}

XTResult xtUserExecuteProgram(
    uint64_t processHandle,
    const char** args,
    const char** envp,
    uint64_t* mainThread
) {
    XTProcess* process = NULL;
    XTProcess* currentProcess = NULL;
    xtGetCurrentProcess(&currentProcess);

    if (processHandle == XT_CURRENT_PROCESS) {
        process = currentProcess;
    }
    else {
        XTDescriptor* desc = NULL;
        XT_TRY(xtGetHandle(currentProcess, processHandle, &desc));
        if (desc->type != XT_DESCRIPTOR_TYPE_PROCESS) return XT_INVALID_PARAMETER;
        process = desc->desc;
    }
    uint64_t argc = 0;
    xtGetArgsCount(args, &argc);
    const char** kargs = NULL;
    xtHeapAlloc(argc * sizeof(const char*), &kargs);
    for (uint64_t i = 0; i < argc; ++i) {
        uint64_t _strlen = 0;
        const char* karg = NULL;
        xtCopyFromUser(&karg, args + i, 8);
        xtUserGetStringLength(karg, &_strlen);
        xtHeapAlloc(_strlen + 1, &kargs[i]);
        xtCopyFromUser(kargs[i], karg, _strlen);
    }
    uint64_t mainThreadId = NULL;
    void* mainThreadPtr = NULL;
    XT_TRY(xtExecuteProgram(
        process,
        kargs,
        NULL,
        &mainThreadPtr
    ));
    xtFindFreeHandle(currentProcess, &mainThreadId);
    XT_TRY(xtDuplicateHandle(currentProcess, mainThreadId, 0, XT_DESCRIPTOR_TYPE_THREAD, mainThreadPtr));
    XT_TRY(xtCopyToUser(mainThread, &mainThreadId, 8));
    return XT_SUCCESS;
}

