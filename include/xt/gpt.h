#ifndef __XT_GPT_H__
#define __XT_GPT_H__

#include <efi/efi.h>
#include <xt/io.h>



XTResult xtSetPartitions(XTFile* blockdev, XTFile** partitions, uint64_t count);

XTResult xtOpenPartitions(XTFile* blockdev, XTFile** out, uint64_t* count);
XTResult xtCreatePartitionDevice(
    XTFile* parentDev, 
    uint64_t first, 
    uint64_t last, 
    uint16_t name[36], 
    EFI_GUID typeGUID, 
    EFI_GUID uniqueGUID,
    XTFile** out
);


#endif