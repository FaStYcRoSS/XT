#include <xt/io.h>

XTResult xtWriteFile(XTFile* file, const void* data, uint64_t* written) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);
    XT_CHECK_ARG_IS_NULL(written);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;

    if (file->IO->WriteFile == NULL) return XT_NOT_IMPLEMENTED;
    
    return file->IO->WriteFile(file, data, written);

}


XTResult xtReadFile(XTFile* file, void* data, uint64_t* read) {
    XT_CHECK_ARG_IS_NULL(file);
    XT_CHECK_ARG_IS_NULL(data);
    XT_CHECK_ARG_IS_NULL(read);

    if (file->IO == NULL) return XT_NOT_IMPLEMENTED;

    if (file->IO->ReadFile == NULL) return XT_NOT_IMPLEMENTED;
    
    return file->IO->ReadFile(file, data, read);
}

XTResult xtWriteToBuffer(XTFile* file, const void* data, uint64_t* written) {
    
}

XTResult xtFlushBuffers(XTFile* file);
