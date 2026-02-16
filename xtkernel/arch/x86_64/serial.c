
#include <xt/arch/x86_64.h>

#include <xt/io.h>
#include <xt/memory.h>

XTFile* gSerialDevice = NULL;

XTResult xtSerialWrite(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written) {

    for (uint64_t i = 0; i < count; ++i) {
        xtOutB(0x3f8, ((char*)data)[i]);
    }
    return XT_SUCCESS;

}

XTFileIO SerialIO = {
    .WriteFile = xtSerialWrite
};

XTFile _gSerialDevice = {
    .data = NULL,
    .IO = &SerialIO
};

XTResult xtSerialInit() {

    gSerialDevice = &_gSerialDevice;
    return XT_SUCCESS;

}
