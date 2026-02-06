
#include <xt/arch/x86_64.h>

#include <xt/io.h>
#include <xt/memory.h>

XTFile* gSerialDevice = NULL;

static XTResult xtSerialWrite(XTFile* file, const void* data, uint64_t* written) {

    for (uint64_t i = 0; i < *written && i < 4096; ++i) {
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
