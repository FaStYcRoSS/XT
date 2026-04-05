
#include <xt/result.h>
#include <xt/kernel.h>
#include <xt/io.h>
#include <xt/memory.h>
#include <xt/string.h>
#include <xt/random.h>
#include <xt/time.h>

typedef struct fat32_ebpb {
    uint8_t jmp[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_logsector;
    uint8_t logsector_per_cluster;
    uint16_t count_reserved;
    uint8_t count_fat_tables;
    uint16_t max_root; //for FAT32 is 0
    uint16_t total_logical_sectors16;
    uint8_t media_descriptor;
    uint16_t log_per_fat;
    uint16_t phys_sector_per_track; //CHS Geomerty. Use 0
    uint16_t phys_heads_per_track; //CHS Geomerty. Use 0
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint32_t log_per_fat32;
    uint16_t mirror_flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t log_fsinfo;
    uint16_t sec_copy;
    uint8_t reserved[12];
    uint8_t physical_drive_number;
    uint8_t f6;
    uint8_t extended_sign;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fstype[8];
    uint8_t reserved_code[420];
    uint16_t magic_word; //0xaa55
} __attribute__((packed)) fat32_ebpb_t;

typedef struct fsinfo {
    uint32_t RRaA;
    uint8_t reserved_code[480];
    uint32_t rrAa;
    uint32_t last_free;
    uint32_t last_allocated;
    uint8_t reserved[12];
    uint32_t boot_sign; //0xaa55
} __attribute__((packed)) fsinfo_t;

typedef struct sfn_dir_entry {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t deleted_time_ms;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t ext_attributes;
    uint16_t last_modifided_time;
    uint16_t last_modifided_date;
    uint32_t first_cluster;
    uint32_t file_size;
} __attribute__((packed)) sfn_dir_entry;

typedef struct lfn_dir_entry {
    uint8_t order;
    uint16_t name0[5];
    uint8_t attributes;
    uint8_t type;
    uint8_t checksum;
    uint16_t name1[6];
    uint16_t firstCluster; //0
    uint16_t name2[2];
}  __attribute__((packed)) lfn_dir_entry;

typedef struct vfatFileData {
    uint32_t index;
    uint32_t parentCluster;
    uint32_t firstCluster;
} vfatFileData;

#define ORed 0x40

#define create_time(hours, minute, second) ((hours << 11) | (minute << 5) | second) 
#define create_date(year, month, day) ((year << 9) | (month << 5) | day) 
#define READONLY 0x1
#define HIDDEN 0x2
#define SYSTEM 0x4
#define VOLUME_LABED 0x8
#define SUBDIR 0x10
#define ARCHIVE 0x20
#define DEV     0x40
#define RESERVED 0x80

#define FREE_CLUSTER 0x00000000
#define RESERVED_FIRST 0x1
#define EOC            0x0fffffff

#define SECTOR_SIZE 512
#define SECTORS_PER_CLUSTER 4
#define RESERVED_SECTORS 32
#define FAT_COUNT 2

typedef struct vfatFSData {
    uint32_t fat_size_bytes;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_sector;
    uint32_t reserved_sectors;
    uint32_t data_offset;
    XTFile*  rootDir;
} vfatFSData;



uint32_t calc_fat_size(uint32_t total_sectors, uint16_t reserved, uint8_t fats, uint8_t spc, uint16_t bps) {
    uint32_t sectors_per_fat = 0;
    uint32_t cluster_count, data_sectors;

    do {
        sectors_per_fat++;
        data_sectors = total_sectors - reserved - (fats * sectors_per_fat);
        cluster_count = data_sectors / spc;
    } while ((cluster_count * 4 + bps - 1) / bps > sectors_per_fat);

    return sectors_per_fat;
}

uint32_t get_next_cluster(XTMountPoint* mp, uint32_t cluster) {
    uint32_t result = 0;
    vfatFSData* fsData = mp->data;
    xtReadFile(mp->device, &result, fsData->reserved_sectors * fsData->bytes_per_sector + (cluster * 4), 4, NULL);
    return result;
}

int set_next_cluster(XTMountPoint* mp, uint32_t to, uint32_t that) {
    vfatFSData* fsData = mp->data;

    xtWriteFile(mp->device, &that, fsData->reserved_sectors * fsData->bytes_per_sector + (to * 4), 4, NULL);
    xtWriteFile(mp->device, &that, fsData->reserved_sectors * fsData->bytes_per_sector + fsData->fat_size_bytes + (to * 4), 4, NULL);
    return 1;
}

uint32_t find_free_cluster(XTMountPoint* mp) {
    vfatFSData* fsData = mp->data;
    for (uint32_t i = 0; i < fsData->fat_size_bytes; i += 4) {
        uint32_t clusterI = 0;
        xtReadFile(mp->device, &clusterI, fsData->bytes_per_sector * fsData->reserved_sectors + i, 4, NULL);
        if (clusterI == 0) {
            return i / 4;
        }
    }
    return 0x0fffffff;
}

XTResult vfatWriteFile(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written) {
    vfatFSData* fsData = file->mountPoint->data;
    vfatFileData* fileData = file->data;
    uint32_t cluster_size = fsData->sectors_per_cluster * fsData->bytes_per_sector;
    uint32_t image_offset = fsData->data_offset;
    uint32_t current_cluster = fileData->firstCluster;
    uint64_t bytes_written = 0;

    // Переход к нужному кластеру
    uint32_t cluster_index = offset / cluster_size;
    for (uint32_t i = 0; i < cluster_index; ++i) {
        uint32_t next = get_next_cluster(file->mountPoint, current_cluster);
        if (next == EOC) {
            next = find_free_cluster(file->mountPoint);
            if (next == EOC) return XT_END_OF_FILE;
            set_next_cluster(file->mountPoint, current_cluster, next);
        }
        current_cluster = next;
    }

    while (bytes_written < count) {
        uint64_t pos_in_cluster = (offset + bytes_written) % cluster_size;
        uint64_t to_write = cluster_size - pos_in_cluster;
        if (to_write > count - bytes_written)
            to_write = count - bytes_written;

        uint64_t phys_offset = image_offset + (current_cluster - 2) * cluster_size + pos_in_cluster;
        uint64_t block_written = 0;
        xtWriteFile(file->mountPoint->device, (const char*)data + bytes_written, phys_offset, to_write, &block_written);
        if (block_written == 0) break;

        bytes_written += block_written;

        // Если дошли до конца кластера, переходим к следующему
        if (pos_in_cluster + block_written == cluster_size) {
            uint32_t next = get_next_cluster(file->mountPoint, current_cluster);
            if (next == EOC) break;
            current_cluster = next;
        }
    }
    if (fileData->parentCluster != EOC) {
        sfn_dir_entry sfn = { 0 };
        uint32_t childCluster = fileData->firstCluster;
        fileData->firstCluster = fileData->parentCluster;
        fileData->parentCluster = EOC;
        xtReadFile(file, &sfn, fileData->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        if (!(sfn.attributes & SUBDIR) && fileData->parentCluster != 0) {
            if (sfn.file_size < (offset + bytes_written)) {
                sfn.file_size = (offset + bytes_written);
            }
            xtWriteFile(file, &sfn, fileData->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        }
        fileData->parentCluster = fileData->firstCluster;
        fileData->firstCluster = childCluster;
    }
    
    *written = bytes_written;
    return (bytes_written == count) ? XT_SUCCESS : XT_END_OF_FILE;
}

XTResult vfatReadFile(XTFile* file, const void* data, uint64_t offset, uint64_t count, uint64_t* written) {
    vfatFSData* fsData = file->mountPoint->data;
    vfatFileData* fileData = file->data;
    uint32_t cluster_size = fsData->sectors_per_cluster * fsData->bytes_per_sector;
    uint32_t image_offset = fsData->data_offset;
    uint32_t current_cluster = fileData->firstCluster;
    uint64_t bytes_written = 0;

    // Переход к нужному кластеру
    uint32_t cluster_index = offset / cluster_size;
    for (uint32_t i = 0; i < cluster_index; ++i) {
        uint32_t next = get_next_cluster(file->mountPoint, current_cluster);
        if (next == EOC) return XT_END_OF_FILE;
        current_cluster = next;
    }

    while (bytes_written < count) {
        uint64_t pos_in_cluster = (offset + bytes_written) % cluster_size;
        uint64_t to_write = cluster_size - pos_in_cluster;
        if (to_write > count - bytes_written)
            to_write = count - bytes_written;

        uint64_t phys_offset = image_offset + (current_cluster - 2) * cluster_size + pos_in_cluster;
        uint64_t block_written = 0;
        xtReadFile(file->mountPoint->device, (const char*)data + bytes_written, phys_offset, to_write, &block_written);
        if (block_written == 0) break;

        bytes_written += block_written;

        // Если дошли до конца кластера, переходим к следующему
        if (pos_in_cluster + block_written == cluster_size) {
            uint32_t next = get_next_cluster(file->mountPoint, current_cluster);
            if (next == EOC) break;
            current_cluster = next;
        }
    }

    *written = bytes_written;
    return (bytes_written == count) ? XT_SUCCESS : XT_END_OF_FILE;
}

XTResult vfatCloseFile(XTFile* file) {
    vfatFileData* fileData = file->data;
    //xtHeapFree(fileData);
    return XT_SUCCESS;
}


XTFileIO vfatFileIO = {
    .WriteFile = vfatWriteFile,
    .ReadFile = vfatReadFile,
    .CloseFile = vfatCloseFile
};

XTResult vfatMakeFS(XTMountPoint* mp) {
    uint64_t sizeOfDisk = 0;
    XTFileInfo info;
    xtGetFileInfo(mp->device, &info);
    sizeOfDisk = info.FileSize;
    uint16_t bps = SECTOR_SIZE;
    uint8_t spc = SECTORS_PER_CLUSTER;
    uint32_t fat_sz = calc_fat_size(sizeOfDisk / SECTOR_SIZE, RESERVED_SECTORS, FAT_COUNT, spc, bps);
    uint32_t fat_bytes = fat_sz * SECTOR_SIZE;
    uint32_t data_start_sector = RESERVED_SECTORS + FAT_COUNT * fat_sz;
    // 3. Boot Sector (EBPB)
    fat32_ebpb_t ebpb = {0};
    ebpb.jmp[0] = 0xEB;
    ebpb.jmp[1] = 0x58;
    ebpb.jmp[2] = 0x90;
    xtCopyMem(ebpb.oem_name, "MSWIN4.1", 8);
    ebpb.bytes_per_logsector = bps;
    ebpb.logsector_per_cluster = spc;
    ebpb.count_reserved = RESERVED_SECTORS;
    ebpb.count_fat_tables = FAT_COUNT;
    ebpb.media_descriptor = 0xF8;
    ebpb.total_sectors32 = sizeOfDisk / SECTOR_SIZE;
    ebpb.log_per_fat32 = fat_sz;
    ebpb.root_cluster = 2;
    ebpb.log_fsinfo = 1;
    ebpb.sec_copy = 6;
    ebpb.extended_sign = 0x29;
    uint64_t random = 0;
    xtGetRandomU64(&random);
    ebpb.volume_id = (uint32_t)random;
    xtCopyMem(ebpb.volume_label, "NO NAME    ", 11);
    xtCopyMem(ebpb.fstype, "FAT32   ", 8);
    ebpb.magic_word = 0xAA55;

    xtWriteFile(mp->device, &ebpb, 0, sizeof(ebpb), NULL);

    // 4. FSINFO
    fsinfo_t fsinfo = {0};
    fsinfo.RRaA = 0x41615252;
    fsinfo.rrAa = 0x61417272;
    fsinfo.last_free = 0x00000004;
    fsinfo.last_allocated = 0xFFFFFFFF;
    fsinfo.boot_sign = 0xAA550000;
    xtWriteFile(mp->device, &fsinfo, SECTOR_SIZE * (ebpb.log_fsinfo), sizeof(fsinfo_t), NULL);

    // 5. FAT #1
    
    uint32_t fati = 0x0FFFFFF8;
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS, 4, NULL);
    fati = 0x0fffffff;
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + 1 * 4, 4, NULL);
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + 2 * 4, 4, NULL);
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + 3 * 4, 4, NULL);

    // 6. FAT #2 (просто копия)
    fati = 0x0FFFFFF8;
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + fat_bytes, 4, NULL);
    fati = 0x0fffffff;
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + 1 * 4 + fat_bytes, 4, NULL);
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + 2 * 4 + fat_bytes, 4, NULL);
    xtWriteFile(mp->device, &fati, SECTOR_SIZE * RESERVED_SECTORS + 3 * 4 + fat_bytes, 4, NULL);
    vfatFSData* fsData = mp->data;
    fsData->fat_size_bytes = ebpb.log_per_fat32 * ebpb.bytes_per_logsector;
    fsData->bytes_per_sector = ebpb.bytes_per_logsector;
    fsData->sectors_per_cluster = ebpb.logsector_per_cluster;
    fsData->reserved_sectors = ebpb.count_reserved;
    fsData->data_offset = ebpb.count_fat_tables * fsData->fat_size_bytes + fsData->bytes_per_sector * fsData->reserved_sectors;
    fsData->rootDir->IO = &vfatFileIO;
    fsData->rootDir->mountPoint = mp;
    vfatFileData* _vfatFileData = fsData->rootDir->data;
    _vfatFileData->firstCluster = ebpb.root_cluster;
    _vfatFileData->index = 0;
    _vfatFileData->parentCluster = EOC;

    // 8. Заполнить конец файла нулями (по необходимости)
    uint8_t zero = 0;
    xtWriteFile(mp->device, &zero, sizeOfDisk - 1, 1, NULL);
    return XT_SUCCESS;
}
uint32_t vfatFindFile(XTMountPoint* mp, const char* path, char** left, XTFile** _dir) {
    if (!mp || !path || !left || !_dir) return 0;

    vfatFSData* fsData = mp->data;
    if (!fsData || !fsData->rootDir) return 0;

    XTFile* currentDir = NULL;
    xtHeapAlloc(sizeof(XTFile), &currentDir);
    currentDir->IO = &vfatFileIO;
    currentDir->mountPoint = mp;
    
    xtHeapAlloc(sizeof(vfatFileData), &currentDir->data);
    vfatFileData* currentData = currentDir->data;
    xtCopyMem(currentData, fsData->rootDir->data, sizeof(vfatFileData));

    const char* p = path;
    
    while (*p) {
        while (*p == '/' || *p == '\\') p++;
        if (*p == '\0') break;

        const char* componentStart = p;
        char name[256];
        uint64_t count = 0;
        while (*p && *p != '/' && *p != '\\' && count < 255) {
            name[count++] = *p++;
        }
        name[count] = '\0';

        // Проверяем, последний ли это компонент
        const char* peek = p;
        while (*peek == '/' || *peek == '\\') peek++;
        int isLast = (*peek == '\0');

        uint64_t offset = 0;
        int found = 0;
        sfn_dir_entry sfn;
        lfn_dir_entry lfnBuffer[20]; 
        int lfnCount = 0;
        uint32_t entryIndex = 0;

        while (xtReadFile(currentDir, &sfn, offset, sizeof(sfn_dir_entry), NULL) == XT_SUCCESS) {
            if (sfn.name[0] == 0x00) break;
            
            if (sfn.name[0] == 0xE5 || sfn.attributes == 0x0F) {
                if (sfn.attributes == 0x0F && lfnCount < 20) {
                    xtCopyMem(&lfnBuffer[lfnCount++], &sfn, sizeof(lfn_dir_entry));
                } else {
                    lfnCount = 0;
                }
                offset += sizeof(sfn_dir_entry);
                entryIndex++;
                continue;
            }
            char fullName[256];
            // ... [Твой код сборки fullName из LFN/SFN] ...
            uint16_t utf16Name[256];
            int pos = 0;
            for (int i = lfnCount - 1; i >= 0; i--) {
                lfn_dir_entry* lfn = &lfnBuffer[i];
                uint16_t wname[13];
                xtCopyMem(wname, lfn->name0, 5 * sizeof(uint16_t));
                xtCopyMem(wname + 5, lfn->name1, 6 * sizeof(uint16_t));
                xtCopyMem(wname + 11, lfn->name2, 2 * sizeof(uint16_t));
                for (int j = 0; j < 13 && pos < 255; j++) {
                    if (wname[j] == 0x0000 || wname[j] == 0xFFFF) break;
                    utf16Name[pos++] = wname[j];
                }
            }

            utf16Name[pos] = 0;
            // Преобразуем UTF-16 в UTF-8
            int outPos = 0;
            for (int i = 0; i < pos; i++) {
                uint32_t cp;
                uint16_t high = utf16Name[i];
                if (high >= 0xD800 && high <= 0xDBFF) { // суррогатная пара
                    uint16_t low = utf16Name[++i];
                    cp = 0x10000 + ((high - 0xD800) << 10) | (low - 0xDC00);
                } else {
                    cp = high;
                }
                if (cp <= 0x7F) {
                    fullName[outPos++] = (uint8_t)cp;
                } else if (cp <= 0x7FF) {
                    fullName[outPos++] = (uint8_t)((cp >> 6) | 0xC0);
                    fullName[outPos++] = (uint8_t)((cp & 0x3F) | 0x80);
                } else if (cp <= 0xFFFF) {
                    fullName[outPos++] = (uint8_t)((cp >> 12) | 0xE0);
                    fullName[outPos++] = (uint8_t)(((cp >> 6) & 0x3F) | 0x80);
                    fullName[outPos++] = (uint8_t)((cp & 0x3F) | 0x80);
                } else {
                    fullName[outPos++] = (uint8_t)((cp >> 18) | 0xF0);
                    fullName[outPos++] = (uint8_t)(((cp >> 12) & 0x3F) | 0x80);
                    fullName[outPos++] = (uint8_t)(((cp >> 6) & 0x3F) | 0x80);
                    fullName[outPos++] = (uint8_t)((cp & 0x3F) | 0x80);
                }

            }
            fullName[outPos] = '\0';
            if (xtStringCmp(name, fullName, 256) == XT_SUCCESS) {
                found = 1;
                if (isLast) {
                    // НАШЛИ ЦЕЛЬ. Останавливаемся в текущей директории.
                    currentData->index = entryIndex; // Сохраняем индекс записи
                    *left = (char*)p; 
                    *_dir = currentDir;
                    return currentData->firstCluster;
                } else {
                    //Это промежуточная директория. Входим в неё.
                    if (!(sfn.attributes & SUBDIR)) {
                        // Ошибка: пытаемся пройти сквоит файл как через папку
                        xtHeapFree(currentDir->data);
                        xtHeapFree(currentDir);
                        return 0;
                    }
                    currentData->firstCluster = sfn.first_cluster;
                    currentData->index = 0; 
                    break; // Переходим к следующему компоненту пути
                }
            }

            lfnCount = 0;
            offset += sizeof(sfn_dir_entry);
            entryIndex++;
        }

        if (!found) {
            // Компонент не найден (нужно для CreateFile)
            *left = (char*)componentStart;
            *_dir = currentDir;
            return currentData->firstCluster;
        }
    }

    *left = (char*)p; 
    *_dir = currentDir;
    return currentData->firstCluster;
}

XTResult xtGetExtension(const char* name, char** ext) {
    uint64_t name_len = 0;
    xtGetStringLength(name, &name_len);
    for (;name_len - 1 > 1 && name[name_len] != '.';--name_len);
    *ext = name + name_len + 1;
    return XT_SUCCESS;
}

XTResult xtGetShortNameLength(const char* name, uint64_t* lenght) {
    uint64_t len = 0;
    for (;*name && *name != '.'; ++name, ++len);
    if (len > 8) len = 8;
    *lenght = len;
    return XT_SUCCESS;
}

uint32_t vfatFindFreeEntry(XTFile* _dir) {
    uint32_t i = 0;
    do {
        sfn_dir_entry sfn = { 0 };
        xtReadFile(_dir, &sfn, i * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        if (sfn.name[0] == '\0' || sfn.name[0] == 0xe5) break;
        ++i;
    } while(1);

    return i;
}

uint8_t calc_checksum(const uint8_t *shortname) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortname[i];
    }
    return sum;
}
XTResult vfatCreateFile(XTMountPoint* mp, const char* name, uint64_t flags) {
    char* left = NULL;
    XTFile* _dir = NULL;
    
    // 1. Ищем путь. vfatFindFile вернет нам родительскую директорию в _dir
    // и имя будущего файла в left.
    vfatFindFile(mp, name, &left, &_dir);

    // Если left пустой, значит файл с таким именем уже существует
    if (left[0] == '\0') {
        xtDebugPrint("File already exists\n");
        if (_dir) { xtHeapFree(_dir->data); xtHeapFree(_dir); }
        return XT_FILE_ALREADY_EXISTS;
    }

    // Проверяем, не является ли left путем (например, "dir1/file.txt")
    // Если в left есть слэш, значит промежуточная директория не была найдена.
    for (char* c = left; *c; c++) {
        if (*c == '/' || *c == '\\') {
            if (_dir) { xtHeapFree(_dir->data); xtHeapFree(_dir); }
            return XT_NOT_FOUND; // Родительская папка не существует
        }
    }

    // 2. Подготовка атрибутов и времени
    uint8_t attr = 0;
    if (!(flags & XT_FILE_MODE_WRITE)) attr |= READONLY;
    if (flags & XT_FILE_ATTRIBUTE_DIRECTORY) attr |= SUBDIR;

    uint64_t current_time = 0;
    xtGetTime(&current_time);
    XTTime time = { 0 };
    xtGetTimeFromUnix(current_time, &time);
    uint16_t dosdate = create_date(time.year, time.month, time.mday);
    uint16_t dostime = create_time(time.hour, time.minutes, time.seconds);

    // 3. Резервируем кластер для нового файла
    uint32_t firstCluster = find_free_cluster(mp);
    set_next_cluster(mp, firstCluster, EOC);

    // 4. Формируем SFN (8.3)
    sfn_dir_entry sfn = {
        .attributes = attr,
        .create_date = dosdate,
        .create_time = dostime,
        .last_modifided_date = dosdate,
        .last_modifided_time = dostime,
        .first_cluster = firstCluster,
        .file_size = 0
    };

    // Заполняем имя пробелами по стандарту FAT
    for (int i = 0; i < 8; i++) sfn.name[i] = ' ';
    for (int i = 0; i < 3; i++) sfn.ext[i] = ' ';

    char* _ext = NULL;
    xtGetExtension(left, &_ext);
    
    uint64_t short_len = 0;
    xtGetShortNameLength(left, &short_len);
    xtCopyMem(sfn.name, left, (short_len > 8) ? 8 : short_len);
    if (_ext && *_ext) {
        uint64_t ext_len = 0;
        xtGetStringLength(_ext, &ext_len);
        xtCopyMem(sfn.ext, _ext, (ext_len > 3) ? 3 : ext_len);
    }

    uint8_t checksum = calc_checksum((uint8_t*)sfn.name);
    uint16_t wname[256];

    uint64_t wname_len = 0;
    // 5. Преобразование имени в UTF-16 для LFN
    for (;*left; ++left) {

        uint32_t cp = 0;

        if (left[0] < 0x80) { // 1-byte ASCII

            cp = left[0];

        } else if ((left[0] & 0xE0) == 0xC0) { // 2-byte

            cp = ((left[0] & 0x1F) << 6) | (left[1] & 0x3F);

            ++left;

        } else if ((left[0] & 0xF0) == 0xE0) { // 3-byte

            cp = ((left[0] & 0x0F) << 12) | ((left[1] & 0x3F) << 6) | (left[2] & 0x3F);

            left += 2;

        } else if ((left[0] & 0xF8) == 0xF0) { // 4-byte

            cp = ((left[0] & 0x07) << 18) | ((left[1] & 0x3F) << 12) | ((left[2] & 0x3F) << 6) | (left[3] & 0x3F);

            left += 3;

        }

        if (cp <= 0xFFFF) {

            wname[wname_len] = (uint16_t)cp;

            wname_len += 1;

        } else { // Surrogate pair for code points > 0xFFFF

            cp -= 0x10000;

            wname[0] = (uint16_t)((cp >> 10) + 0xD800);

            wname[1] = (uint16_t)((cp & 0x3FF) + 0xDC00);

            wname_len += 2;

        }

    }
    // 6. Запись в директорию
    uint64_t countOfLFNEntries = (wname_len + 12) / 13;
    uint32_t free_index = vfatFindFreeEntry(_dir);
    uint64_t lfn_pos = 0;
    for (uint64_t i = 1; i <= countOfLFNEntries && lfn_pos < wname_len; ++i) {

        uint8_t order = (countOfLFNEntries - i); // убывает от N до 1

        if (i == 1) order |= ORed; // последняя в файле (первая по порядку) имеет бит

        lfn_dir_entry lfn = {

            .firstCluster = 0,

            .attributes = 0xf,

            .order = order,

            .checksum = checksum

        };

        uint64_t remaining = wname_len - lfn_pos;
        // Для name0 (5 символов)
        uint64_t to_copy0 = (remaining < 5) ? remaining : 5;
        for (uint64_t k = 0; k < 5; k++) {
            lfn.name0[k] = (k < to_copy0) ? wname[lfn_pos + k] : 0xFFFF;
        }
        lfn_pos += to_copy0;
        remaining -= to_copy0;

        // Для name1 (6 символов)
        uint64_t to_copy1 = (remaining < 6) ? remaining : 6;
        for (uint64_t k = 0; k < 6; k++) {
            lfn.name1[k] = (k < to_copy1) ? wname[lfn_pos + k] : 0xFFFF;
        }
        lfn_pos += to_copy1;
        remaining -= to_copy1;

        // Для name2 (2 символа)
        uint64_t to_copy2 = (remaining < 2) ? remaining : 2;
        for (uint64_t k = 0; k < 2; k++) {
            lfn.name2[k] = (k < to_copy2) ? wname[lfn_pos + k] : 0xFFFF;
        }

        xtWriteFile(_dir, &lfn, (free_index + (countOfLFNEntries - i)) * sizeof(lfn_dir_entry), sizeof(lfn_dir_entry), NULL);

    }


    // Пишем финальную SFN запись
    xtWriteFile(_dir, &sfn, (free_index + countOfLFNEntries) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);

    // 7. Чистим временный объект директории
    xtHeapFree(_dir->data);
    xtHeapFree(_dir);

    return XT_SUCCESS;
}

XTResult vfatOpenFile(XTMountPoint* mp, const char* name, uint64_t flags, XTFile** out) {
    char* left = NULL;
    XTFile* parentDir = NULL;
    vfatFindFile(mp, name, &left, &parentDir);

    if (xtStringCmp(left, "", 1) != XT_SUCCESS) {
        // Если путь не разобран до конца, файла нет

        xtDebugPrint("left %s\n", left);
        if (parentDir) { xtHeapFree(parentDir->data); xtHeapFree(parentDir); }
        return XT_NOT_FOUND;
    }
    vfatFileData* data = parentDir->data;
    uint32_t parentCluster = data->firstCluster;
    sfn_dir_entry sfn;
    // Читаем данные файла по найденному индексу
    xtReadFile(parentDir, &sfn, data->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);

    // Теперь ПЕРЕНАСТРАИВАЕМ parentDir так, чтобы он стал самим файлом
    data->firstCluster = sfn.first_cluster;
    // data->fileSize = sfn.file_size; // Если бы поле было, но ты используешь xtGetFileInfo
    // data->index = 0; // Сбрасываем для чтения контента файла
    data->parentCluster = parentCluster;
    *out = parentDir;
    return XT_SUCCESS;
}

XTResult vfatMount(XTMountPoint* mp) {
    vfatFSData* fsData = NULL;
    xtHeapAlloc(sizeof(vfatFSData), &fsData);
    fat32_ebpb_t ebpb = { 0 };
    XT_TRY(xtReadFile(mp->device, &ebpb, 0, sizeof(fat32_ebpb_t), NULL));
    fsData->fat_size_bytes = ebpb.log_per_fat32 * ebpb.bytes_per_logsector;
    fsData->bytes_per_sector = ebpb.bytes_per_logsector;
    fsData->sectors_per_cluster = ebpb.logsector_per_cluster;
    fsData->reserved_sectors = ebpb.count_reserved;
    fsData->data_offset = ebpb.count_fat_tables * fsData->fat_size_bytes + fsData->bytes_per_sector * fsData->reserved_sectors;
    xtHeapAlloc(sizeof(XTFile), &fsData->rootDir);
    fsData->rootDir->IO = &vfatFileIO;
    fsData->rootDir->mountPoint = mp;
    xtHeapAlloc(sizeof(vfatFileData), &fsData->rootDir->data);

    vfatFileData* _vfatFileData = fsData->rootDir->data;
    _vfatFileData->firstCluster = 0x2;
    _vfatFileData->index = 0;
    _vfatFileData->parentCluster = EOC;
    mp->data = fsData;
    return XT_SUCCESS;
}

XTFileSystemIO vfatIO = {
    .MakeFS = vfatMakeFS,
    .Mount = vfatMount,
    .CreateFile = vfatCreateFile,
    .OpenFile = vfatOpenFile
};

XTFileSystem vfat = {
    .Name = "vfat",
    .IO = &vfatIO
};

XTResult xtVFATInit() {
    return xtRegisterFileSystem(&vfat);
}

XTResult __attribute__((weak)) xtDriverMain(int reason) {
    return xtVFATInit();
}
