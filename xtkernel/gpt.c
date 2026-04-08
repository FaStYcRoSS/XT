#include <xt/random.h>
#include <xt/memory.h>
#include <xt/string.h>
#include <xt/gpt.h>

typedef struct MBR_PARTITION {
    uint8_t status;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t lba_size;
} __attribute__((packed)) MBR_PARTITION;

typedef struct MBR {
    uint8_t code[446];
    MBR_PARTITION partitions[4];
    uint16_t magic_code;
} __attribute__((packed)) MBR;

typedef struct GPT_Header {
    uint8_t signature[8]; //'EFI PART'
    uint32_t revision;
    uint32_t headerSize;
    uint32_t crc32_checksum;
    uint32_t reserved;
    uint64_t currentLba;
    uint64_t backupLba;
    uint64_t firstLba;
    uint64_t lastLba;
    EFI_GUID diskGUID;
    uint64_t startPartitions;
    uint32_t countOfPartitions;
    uint32_t sizeofPartition;
    uint32_t crc32_PartCheckSum;
} __attribute__((packed)) GPT_Header;

typedef struct GPT_Partition {
    EFI_GUID PartitionType;
    EFI_GUID uniqueGUID;
    uint64_t firstLba;
    uint64_t lastLba;
    uint64_t attributes;
    uint16_t Name[36];
} __attribute__((packed)) GPT_Partition;

typedef struct XTPartitionDev {
    GPT_Partition gptPart;
    XTFile* parent;
} XTPartitionDev;

XTResult xtPartWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written) {
    XTPartitionDev* partData = file->data;
    offset += partData->gptPart.firstLba * 512;
    return xtWriteFile(partData->parent, data, offset, count, written);
}

XTResult xtPartReadFile(XTFile* file, void* data, uint64_t offset, uint64_t count, uint64_t* written) {
    XTPartitionDev* partData = file->data;
    offset += partData->gptPart.firstLba * 512;
    return xtReadFile(partData->parent, data, offset, count, written);
}

XTResult xtPartGetFileInfo(XTFile* file, XTFileInfo* info) {
    XTPartitionDev* partData = file->data;
    info->FileSize = (partData->gptPart.lastLba - partData->gptPart.firstLba + 1) * 512;
    return XT_SUCCESS;
}

XTFileIO partIO = {
    .WriteFile = xtPartWriteFile,
    .ReadFile = xtPartReadFile,
    .GetFileInfo = xtPartGetFileInfo
};




XTResult xtGetMaxSizeOfPartitions(XTFile** array, uint64_t count, uint64_t* size) {
    uint64_t maxSize = 0;
    for (uint64_t i = 0; i < count; ++i) {
        XTPartitionDev* part = array[i]->data;
        if (maxSize < part->gptPart.lastLba) {
            maxSize = part->gptPart.lastLba;
        }
    }
    *size = maxSize;
    return XT_SUCCESS;
}

XTResult LBA2CHS(uint32_t lba, uint8_t chs[3]) {
    uint8_t temp = lba / 63;
    chs[1] = ((lba % 63) + 1) | ((temp / 255) >> 8) << 6;
    chs[0] = temp % 255;
    chs[2] = temp / 255;
    return XT_SUCCESS;
}

EFI_GUID new_guid(void) {
    uint8_t rand_arr[16] = { 0 };

    for (uint8_t i = 0; i < 1; i++) {
        xtGetRandomU64(&rand_arr[i*8]);
    }

    // Fill out GUID
    EFI_GUID result = {
        .Data1         = *(uint32_t *)&rand_arr[0],
        .Data2        = *(uint16_t *)&rand_arr[4],
        .Data3 = *(uint16_t *)&rand_arr[6],
        .Data4 = { rand_arr[7], rand_arr[8], rand_arr[9], rand_arr[10], rand_arr[11], rand_arr[12], rand_arr[13], rand_arr[14], rand_arr[15] },
    };

    // Fill out version bits - version 4
    result.Data3 &= ~(1 << 15); // 0b_0_111 1111
    result.Data3 |= (1 << 14);  // 0b0_1_00 0000
    result.Data3 &= ~(1 << 13); // 0b11_0_1 1111
    result.Data3 &= ~(1 << 12); // 0b111_0_ 1111

    // Fill out variant bits
    result.Data4[0] |= (1 << 7);    // 0b_1_000 0000
    result.Data4[0] |= (1 << 6);    // 0b0_1_00 0000
    result.Data4[0] &= ~(1 << 5);   // 0b11_0_1 1111

    return result;
}


XTResult xtCreatePartitionDevice(
    XTFile* parentDev, 
    uint64_t first, 
    uint64_t last, 
    uint16_t name[36], 
    EFI_GUID typeGUID, 
    EFI_GUID uniqueGUID,
    XTFile** out
) {
    XTPartitionDev* partDev = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTPartitionDev), &partDev));
    partDev->gptPart.attributes = 0x0;
    partDev->gptPart.firstLba = first;
    partDev->gptPart.lastLba = last;
    xtCopyMem(partDev->gptPart.Name, name, 36 * sizeof(uint16_t));
    EFI_GUID nullGUID = { 0, 0, 0, {0, 0, 0, 0}};
    if (xtCompareMemory(&uniqueGUID, &nullGUID, sizeof(EFI_GUID)) == XT_SUCCESS) {
        uniqueGUID = new_guid();
    }
    xtCopyMem(&partDev->gptPart.uniqueGUID, &uniqueGUID, sizeof(EFI_GUID));
    xtCopyMem(&partDev->gptPart.PartitionType, &typeGUID, sizeof(EFI_GUID));
    partDev->parent = parentDev;
    XTFile* file = NULL;
    XT_TRY(xtHeapAlloc(sizeof(XTFile), &file));
    file->data = partDev;
    file->IO = &partIO;
    file->mountPoint = NULL;
    *out = file;
    return XT_SUCCESS;
}

uint32_t crc_table[256];

void create_crc32_table(void) {
    uint32_t c = 0;

    for (int32_t n = 0; n < 256; n++) {
        c = (uint32_t)n;
        for (uint8_t k = 0; k < 8; k++) {
            if (c & 1) 
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[n] = c;
    }
}

// =====================================
// Calculate CRC32 value for range of data
// =====================================
uint32_t calculate_crc32(void *buf, int32_t len) {
    static int made_crc_table = 0;

    uint8_t *bufp = buf;
    uint32_t c = 0xFFFFFFFFL;

    if (!made_crc_table) {
        create_crc32_table();
        made_crc_table = 1;
    }

    for (int32_t n = 0; n < len; n++) 
        c = crc_table[(c ^ bufp[n]) & 0xFF] ^ (c >> 8);

    // Invert bits for return value
    return c ^ 0xFFFFFFFFL;
}

XTResult xtSetPartitions(XTFile* blockdev, XTFile** array, uint64_t count) {
    uint64_t maxSize = 0;
    xtGetMaxSizeOfPartitions(array, count, &maxSize);
    uint8_t chsEnd[3] = { 0};
    LBA2CHS(maxSize & 0xffffffff, chsEnd);
    MBR mbr = { 
        .code =
        {
            0xbb, 0x12, 0x7c, 0xb4, 0x0e, 0x8a, 0x07, 0xcd, 
            0x10, 0x3c, 0x00, 0x74, 0x03, 0x43, 0xeb, 0xf3, 
            0xeb, 0xfe, 0x54, 0x68, 0x69, 0x73, 0x20, 0x64, 
            0x69, 0x73, 0x6b, 0x20, 0x64, 0x6f, 0x65, 0x73, 
            0x6e, 0x27, 0x74, 0x20, 0x6c, 0x6f, 0x61, 0x64, 
            0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20, 0x4d, 0x42, 
            0x52, 0x00, 0x00, 
        },
        .magic_code = 0xaa55,
        .partitions = {
            [0] = {
                .status = 0x00,
                .type = 0xee,
                .chs_start = {0x00, 0x02, 0x00},
                .chs_end = chsEnd,
                .lba_start = 0x1,
                .lba_size = -1
            },
        }
    };
    uint64_t written = 0;
    xtWriteFile(blockdev, &mbr, 0, sizeof(MBR), &written);
    GPT_Header gptHeader = {
        .signature = "EFI PART",
        .currentLba = 0x1,
        .firstLba = 0x22,
        .lastLba = maxSize - 0x1,
        .backupLba = maxSize - 0x2,
        .countOfPartitions = 128,
        .diskGUID = new_guid(),
        .crc32_checksum = 0,
        .crc32_PartCheckSum = 0,
        .sizeofPartition = sizeof(GPT_Partition),
        .revision = 0x00010000,
        .headerSize = 92,
        .startPartitions = 0x2
    };


    GPT_Partition* parts = NULL;
    xtHeapAlloc(sizeof(GPT_Partition) * 128, &parts);
    xtSetMem(parts, 0, sizeof(GPT_Partition) * 128);
    for (uint64_t i = 0; i < count; ++i) {
        XTPartitionDev* partDev = array[i]->data;
        EFI_GUID nullGUID = { 0 };
        if (xtCompareMemory(&partDev->gptPart.uniqueGUID, &nullGUID, sizeof(EFI_GUID)) == XT_SUCCESS) {
            partDev->gptPart.uniqueGUID = new_guid();
        }
        xtCopyMem(parts + i, &partDev->gptPart, sizeof(GPT_Partition));
    }
    gptHeader.crc32_PartCheckSum = calculate_crc32(parts, sizeof(GPT_Partition) * 128);
    uint32_t sum = calculate_crc32(&gptHeader, sizeof(GPT_Header));
    gptHeader.crc32_checksum = sum;
    xtWriteFile(blockdev, &gptHeader, 512, sizeof(GPT_Header), &written);
    xtWriteFile(blockdev, parts, 2 * 512, sizeof(GPT_Partition) * 128, &written);
    xtHeapFree(parts);
    return XT_SUCCESS;
}

XTResult xtOpenPartitions(XTFile* blockdev, XTFile** out, uint64_t* count) {

}


