
#include <xt/result.h>
#include <xt/kernel.h>
#include <xt/io.h>
#include <xt/memory.h>
#include <xt/string.h>
#include <xt/random.h>
#include <xt/time.h>
#include <xt/encodings.h>

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
    uint16_t firstClusterHigh;
    uint16_t last_modifided_time;
    uint16_t last_modifided_date;
    uint16_t firstClusterLow;
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

#define _create_time(hours, minute, second) ((hours << 11) | (minute << 5) | (second >> 1)) 
#define _create_date(year, month, day) (((year) << 9) | ((month) << 5) | (day)) 

#define get_year(date) ((date) >> 9)
#define get_month(date) (((date) >> 5) & 0xf)
#define get_mday(date) ((date) & 0x1f)
#define get_seconds(time) (((time) & 0x1f) << 1)
#define get_minutes(time) (((time) >> 5) & 0x3f)
#define get_hours(time) ((time) >> 11)

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
    if (cluster < 2) return EOC;
    vfatFSData* fsData = mp->data;
    uint32_t result = 0;
    XTResult res = xtReadFile(mp->device, &result,
               fsData->reserved_sectors * fsData->bytes_per_sector + cluster * 4,
               4, NULL);
    if (res != XT_SUCCESS) return EOC;
    return result;
}
int set_next_cluster(XTMountPoint* mp, uint32_t from, uint32_t to) {
    if (from < 2 || to == 0 || to == 1) return 0;   // to не должен быть 0 или 1
    vfatFSData* fsData = mp->data;
    uint32_t fat_offset = fsData->reserved_sectors * fsData->bytes_per_sector;
    xtDebugPrint("fat_offset %x\n", fat_offset);
    xtWriteFile(mp->device, &to, fat_offset + from * 4, 4, NULL);
    xtWriteFile(mp->device, &to, fat_offset + fsData->fat_size_bytes + from * 4, 4, NULL);
    return 1;
}

uint32_t find_free_cluster(XTMountPoint* mp) {
    vfatFSData* fsData = mp->data;
    uint32_t max_cluster = fsData->fat_size_bytes / 4;  // общее количество записей
    for (uint32_t i = 2; i < max_cluster; ++i) {       // начинаем с кластера 2
        uint32_t cluster_val = 0;
        xtReadFile(mp->device, &cluster_val,
                   fsData->bytes_per_sector * fsData->reserved_sectors + i * 4,
                   4, NULL);
        if (cluster_val == 0) {
            return i;
        }
    }
    return EOC;   // 0x0FFFFFFF
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
            set_next_cluster(file->mountPoint, next, EOC);
        }
        current_cluster = next;
    }
    while (bytes_written < count) {
        uint64_t pos_in_cluster = (offset + bytes_written) % cluster_size;
        uint64_t to_write = cluster_size - pos_in_cluster;
        if (to_write > count - bytes_written)
            to_write = count - bytes_written;

        uint64_t phys_offset = image_offset + (uint64_t)(current_cluster - 2) * cluster_size + pos_in_cluster;
        uint64_t block_written = 0;
        xtWriteFile(file->mountPoint->device, (const char*)data + bytes_written, phys_offset, to_write, &block_written);
        if (block_written == 0) break;

        bytes_written += block_written;

        // Если дошли до конца кластера, переходим к следующему
        if (pos_in_cluster + block_written == cluster_size) {
            uint32_t next = get_next_cluster(file->mountPoint, current_cluster);
            if (next == EOC) {
                next = find_free_cluster(file->mountPoint);
                if (next == EOC) return XT_END_OF_FILE;
                set_next_cluster(file->mountPoint, current_cluster, next);
                set_next_cluster(file->mountPoint, next, EOC);
            }
            current_cluster = next;
        }
    }
    if (fileData->parentCluster != EOC) {
        sfn_dir_entry sfn = { 0 };
        uint32_t childCluster = fileData->firstCluster;
        fileData->firstCluster = fileData->parentCluster;
        fileData->parentCluster = EOC;
        xtReadFile(file, &sfn, fileData->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        if (!(sfn.attributes & SUBDIR)) {
            if (sfn.file_size < (offset + bytes_written)) {
                sfn.file_size = (offset + bytes_written);
            }
            uint64_t current_time = 0;
            uint64_t current_time_nanoseconds = 0;
            xtGetTime(&current_time, &current_time_nanoseconds);
            XTTime tm = { 0 };
            xtGetTimeFromUnix(current_time, &tm);
            sfn.last_modifided_time = _create_time(tm.hour, tm.minutes, tm.seconds);
            sfn.last_modifided_date = _create_date(tm.year - 1980, tm.month, tm.mday);
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

XTResult vfatCloseFile(XTMountPoint* mp, XTFile* file) {
    vfatFileData* fileData = file->data;
    xtHeapFree(fileData);
    return XT_SUCCESS;
}


XTResult vfatSFNtoFileInfo(sfn_dir_entry* sfn, XTFileInfo* info) {
    XTTime tm = {
        .year = get_year(sfn->create_date) + 1980,
        .month = get_month(sfn->create_date),
        .mday = get_mday(sfn->create_date),
        .hour = get_hours(sfn->create_time),
        .minutes = get_minutes(sfn->create_time),
        .seconds = get_seconds(sfn->create_time)
    };
    uint64_t create_time_utc = 0;
    xtMakeTime(&tm, &info->createdTime);
    tm.year = get_year(sfn->last_modifided_date) + 1980;
    tm.month = get_month(sfn->last_modifided_date);
    tm.mday = get_mday(sfn->last_modifided_date);
    tm.hour = get_hours(sfn->last_modifided_time);
    tm.minutes = get_minutes(sfn->last_modifided_time);
    tm.seconds = get_seconds(sfn->last_modifided_time);
    xtMakeTime(&tm, &info->lastWriteTime);
    info->flags |= XT_FILE_MODE_READ;
    if (sfn->attributes & SUBDIR) {
        info->flags |= XT_FILE_ATTRIBUTE_DIRECTORY;
    }
    if (!(sfn->attributes & READONLY)) {
        info->flags |= XT_FILE_MODE_WRITE;
    }
    return XT_SUCCESS;
}

XTResult vfatFileInfoToSFN(sfn_dir_entry* sfn, XTFileInfo* info) {
    XTTime tm = { 0 };
    xtGetTimeFromUnix(info->createdTime, &tm);
    sfn->create_time = _create_time(tm.hour, tm.minutes, tm.seconds);
    sfn->create_date = _create_date(tm.year - 1980, tm.month, tm.mday);
    xtGetTimeFromUnix(info->lastWriteTime, &tm);
    sfn->last_modifided_time = _create_time(tm.hour, tm.minutes, tm.seconds);
    sfn->last_modifided_date = _create_date(tm.year - 1980, tm.month, tm.mday);
    if (info->flags & XT_FILE_ATTRIBUTE_DIRECTORY) {
        sfn->attributes |= SUBDIR;
    }
    return XT_SUCCESS;
}

XTResult xtGetExtension(const char* name, char** ext) {
    uint64_t name_len = 0;
    xtGetStringLength(name, &name_len);
    for (;(name_len - 1 > 1) && name[name_len] != '.';--name_len);
    if (name_len == 0) *ext = NULL;
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

uint8_t calc_checksum(const uint8_t *shortname) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortname[i];
    }
    return sum;
}

uint8_t to_upper(uint8_t ascii) {
    if (ascii <= 'z' && ascii >= 'a') return ascii - 'a' + 'A';
    return ascii;
}

XTResult vfatSetFileInfo(XTFile* file, XTFileInfo* info) {
    vfatFileData* fileData = file->data;
    vfatFSData* fsData = file->mountPoint->data;
    sfn_dir_entry sfn = { 0 };
    uint32_t childCluster = fileData->firstCluster;
    fileData->firstCluster = fileData->parentCluster;
    fileData->parentCluster = EOC;
    xtReadFile(file, &sfn, fileData->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
    vfatFileInfoToSFN(&sfn, info);
    sfn.file_size = info->FileSize;
    sfn.firstClusterLow = childCluster & 0xffff;
    sfn.firstClusterHigh = (childCluster >> 16);

    sfn_dir_entry temp_sfn = { 0 };
    int lfnCount = 0;
    uint8_t order = 0;
    do {

        xtReadFile(file, &temp_sfn, (fileData->index - lfnCount - 1) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        order = temp_sfn.name[0];
        if (temp_sfn.attributes == 0xf) {
            temp_sfn.name[0] = 0xe5;
            xtWriteFile(file, &temp_sfn, (fileData->index - lfnCount - 1) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
            ++lfnCount;
        }
    } while(temp_sfn.attributes == 0xf && !(order & ORed));
    uint32_t free_index = fileData->index - lfnCount;
    // Заполняем имя пробелами по стандарту FAT
    for (int i = 0; i < 8; i++) sfn.name[i] = ' ';
    for (int i = 0; i < 3; i++) sfn.ext[i] = ' ';

    char* _ext = NULL;
    xtGetExtension(info->name, &_ext);
    
    uint64_t short_len = 0;
    xtGetShortNameLength(info->name, &short_len);
    uint64_t count = (short_len > 8) ? 8 : short_len;
    for (uint64_t i = 0; i < count; ++i) {
        sfn.name[i] = to_upper(info->name[i]);
    }
    if (_ext && *_ext && (sfn.attributes & SUBDIR)) {
        uint64_t ext_len = 0;
        xtGetStringLength(_ext, &ext_len);
        uint64_t count = (ext_len > 3) ? 3 : ext_len;
        for (uint64_t i = 0; i < count; ++i) {
            sfn.ext[i] = to_upper(_ext[i]);
        }
    }

    uint8_t checksum = calc_checksum((uint8_t*)sfn.name);
    uint16_t wname[256];

    uint64_t wname_len = 0;
    // 5. Преобразование имени в UTF-16 для LFN
    xtUTF8toUTF16(info->name, wname);
    for (uint16_t* i = wname; *i; ++i, ++wname_len);
    // 6. Запись в директорию
    uint64_t countOfLFNEntries = (wname_len + 12) / 13;

    uint64_t lfn_pos = 0;
    for (uint64_t i = 1; i <= countOfLFNEntries && lfn_pos < wname_len; ++i) {

        uint8_t order = (countOfLFNEntries - i + 1); // убывает от N до 1

        if (i == 1) order |= ORed; // последняя в файле (первая по порядку) имеет бит

        lfn_dir_entry lfn = { 0 };
        xtSetMem(&lfn, 0, 32);
        lfn.attributes = 0xf;
        lfn.firstCluster = 0;
        lfn.type = 0;
        lfn.order = order;
        lfn.checksum = checksum;

        uint64_t remaining = wname_len - lfn_pos;
        // Для name0 (5 символов)
        uint64_t to_copy0 = (remaining < 5) ? remaining : 5;
        for (uint64_t k = 0; k < 5; k++) {
            if (k < to_copy0) lfn.name0[k] = wname[lfn_pos + k];
            else if (k == to_copy0 && lfn_pos + k == wname_len) lfn.name0[k] = 0x0000; // Терминатор
            else lfn.name0[k] = 0xffff; // Паддинг
        }
        lfn_pos += to_copy0;
        remaining -= to_copy0;

        // Для name1 (6 символов)
        uint64_t to_copy1 = (remaining < 6) ? remaining : 6;
        for (uint64_t k = 0; k < 6; k++) {
            if (k < to_copy1) lfn.name1[k] = wname[lfn_pos + k];
            else if (k == to_copy1 && lfn_pos + k == wname_len) lfn.name1[k] = 0x0000;
            else lfn.name1[k] = 0xffff;
        }
        lfn_pos += to_copy1;
        remaining -= to_copy1;

        // Для name2 (2 символа)
        uint64_t to_copy2 = (remaining < 2) ? remaining : 2;
        for (uint64_t k = 0; k < 2; k++) {
            if (k < to_copy2) lfn.name2[k] = wname[lfn_pos + k];
            else if (k == to_copy2 && lfn_pos + k == wname_len) lfn.name2[k] = 0x0000;
            else lfn.name2[k] = 0xffff;
        }

        xtWriteFile(file, &lfn, (free_index + (countOfLFNEntries - i)) * sizeof(lfn_dir_entry), sizeof(lfn_dir_entry), NULL);

    }

    // Пишем финальную SFN запись
    xtWriteFile(file, &sfn, (free_index + countOfLFNEntries) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
    fileData->parentCluster = fileData->firstCluster;
    fileData->firstCluster = childCluster;

    return XT_SUCCESS;
}

XTResult vfatGetFileInfo(XTFile* file, XTFileInfo* info) {
    vfatFileData* fileData = file->data;
    vfatFSData* fsData = file->mountPoint->data;
    sfn_dir_entry sfn = { 0 };
    uint32_t childCluster = fileData->firstCluster;
    fileData->firstCluster = fileData->parentCluster;
    fileData->parentCluster = EOC;
    xtReadFile(file, &sfn, fileData->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
    info->FileSize = sfn.file_size;
    info->PhysicalSize = 
        (sfn.file_size / (fsData->bytes_per_sector * fsData->sectors_per_cluster) + 1) * 
        (fsData->bytes_per_sector * fsData->sectors_per_cluster);
    vfatSFNtoFileInfo(&sfn, info);
    lfn_dir_entry lfnBuffer[20];
    int lfnCount = 0;
    do {
        xtReadFile(file, &sfn, (fileData->index - lfnCount - 1) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        if (sfn.attributes == 0xf) {
            xtCopyMem(&lfnBuffer[lfnCount++], &sfn, sizeof(lfn_dir_entry));
        }
    } while(sfn.attributes == 0xf && !(sfn.name[0] & ORed));
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
    xtUTF16toUTF8(utf16Name, info->name);
    fileData->parentCluster = fileData->firstCluster;
    fileData->firstCluster = childCluster;
    return XT_SUCCESS;
}

XTFileIO vfatFileIO = {
    .WriteFile = vfatWriteFile,
    .ReadFile = vfatReadFile,
    .CloseFile = vfatCloseFile,
    .GetFileInfo = vfatGetFileInfo,
    .SetFileInfo = vfatSetFileInfo,
};

XTResult vfatMakeFS(XTFile* file) {
    _Static_assert(sizeof(fat32_ebpb_t) == 512, "Invalid EBPB size");
    uint64_t sizeOfDisk = 0;
    XTFileInfo info;
    xtGetFileInfo(file, &info);
    sizeOfDisk = info.FileSize;
    xtDebugPrint("sizeOfDisk %llu\n", sizeOfDisk);
    uint16_t bps = SECTOR_SIZE;
    uint8_t spc = SECTORS_PER_CLUSTER;
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
    uint32_t total_sectors = sizeOfDisk / SECTOR_SIZE;
    uint32_t cluster_size_sectors = spc;
    uint32_t data_sectors = total_sectors - RESERVED_SECTORS; // предварительно
    // Округляем data_sectors до числа, кратного spc, вычитая остаток
    uint32_t remainder = data_sectors % cluster_size_sectors;
    if (remainder != 0) {
        data_sectors -= remainder;
        total_sectors = RESERVED_SECTORS + data_sectors;
        // Принудительно уменьшаем размер раздела в восприятии FAT
        // (физически раздел может быть больше, но FAT будет использовать только эту часть)
    }
    uint32_t fat_sz = calc_fat_size(total_sectors, RESERVED_SECTORS, FAT_COUNT, spc, bps);
    ebpb.total_sectors32 = total_sectors;
    ebpb.log_per_fat32 = fat_sz;
    xtDebugPrint("ebpb.total_sectors32 %u log_per_fat32 %u\n", ebpb.total_sectors32, ebpb.log_per_fat32);
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

    xtWriteFile(file, &ebpb, 0, sizeof(fat32_ebpb_t), NULL);
    xtWriteFile(file, &ebpb, 6 * SECTOR_SIZE, sizeof(fat32_ebpb_t), NULL);

    // 4. FSINFO
    fsinfo_t fsinfo = {0};
    fsinfo.RRaA = 0x41615252;
    fsinfo.rrAa = 0x61417272;
    fsinfo.last_free = 0x00000003;
    fsinfo.last_allocated = 0xFFFFFFFF;
    fsinfo.boot_sign = 0xAA550000;
    xtWriteFile(file, &fsinfo, SECTOR_SIZE * (ebpb.log_fsinfo), sizeof(fsinfo_t), NULL);
    uint16_t fsinfo_sig = 0xAA55;
    xtWriteFile(file, &fsinfo_sig, SECTOR_SIZE * ebpb.log_fsinfo + 510, 2, NULL);
    uint32_t fat_bytes = fat_sz * bps;
    // 5. FAT #1
    
    uint32_t fati = 0x0FFFFFF8;
    xtWriteFile(file, &fati, SECTOR_SIZE * RESERVED_SECTORS, 4, NULL);
    fati = 0x0fffffff;
    xtWriteFile(file, &fati, SECTOR_SIZE * RESERVED_SECTORS + 1 * 4, 4, NULL);
    xtWriteFile(file, &fati, SECTOR_SIZE * RESERVED_SECTORS + 2 * 4, 4, NULL);

    // 6. FAT #2 (просто копия)
    fati = 0x0FFFFFF8;
    xtWriteFile(file, &fati, SECTOR_SIZE * RESERVED_SECTORS + fat_bytes, 4, NULL);
    fati = 0x0fffffff;
    xtWriteFile(file, &fati, SECTOR_SIZE * RESERVED_SECTORS + 1 * 4 + fat_bytes, 4, NULL);
    xtWriteFile(file, &fati, SECTOR_SIZE * RESERVED_SECTORS + 2 * 4 + fat_bytes, 4, NULL);

    // 8. Заполнить конец файла нулями (по необходимости)
    uint8_t zero = 0;
    xtWriteFile(file, &zero, sizeOfDisk - 1, 1, NULL);
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

        const char* peek = p;
        while (*peek == '/' || *peek == '\\') peek++;
        int isLast = (*peek == '\0');

        uint64_t offset = 0;
        uint32_t entryIndex = 0;
        int found = 0;
        sfn_dir_entry sfn;
        lfn_dir_entry lfnBuffer[20];
        int lfnCount = 0;

        while (xtReadFile(currentDir, &sfn, offset, sizeof(sfn_dir_entry), NULL) == XT_SUCCESS) {
            if (sfn.name[0] == 0x00) break;
            
            if (sfn.name[0] == 0xE5 || sfn.attributes == 0x0F) {
                if (sfn.attributes == 0x0F && lfnCount < 20) {
                    xtCopyMem(&lfnBuffer[lfnCount++], &sfn, sizeof(lfn_dir_entry));
                } else {
                    lfnCount = 0;
                }
            } else {
                char fullName[256];
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
                xtUTF16toUTF8(utf16Name, fullName);
                if (xtStringCmp(name, fullName, 256) == XT_SUCCESS) {
                    found = 1;
                    if (isLast) {
                        currentData->index = entryIndex;
                        *left = (char*)p;
                        *_dir = currentDir;
                        return currentData->firstCluster;
                    } else {
                        if (!(sfn.attributes & SUBDIR)) {
                            xtHeapFree(currentDir->data);
                            xtHeapFree(currentDir);
                            return 0;
                        }
                        currentData->firstCluster = sfn.firstClusterLow | ((uint32_t)sfn.firstClusterHigh << 16);
                        currentData->index = 0;
                        break;
                    }
                }
                lfnCount = 0;
            }
            offset += sizeof(sfn_dir_entry);
            entryIndex++;
        }

        if (!found) {
            *left = (char*)componentStart;
            *_dir = currentDir;
            return currentData->firstCluster;
        }
    }

    *left = (char*)p;
    *_dir = currentDir;
    return currentData->firstCluster;
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


XTResult vfatCreateFile(XTMountPoint* mp, const char* name, uint64_t flags) {
    char* left = NULL;
    XTFile* _dir = NULL;
    vfatFSData* fsData = mp->data;
    // 1. Ищем путь. vfatFindFile вернет нам родительскую директорию в _dir
    // и имя будущего файла в left.
    vfatFindFile(mp, name, &left, &_dir);
    vfatFileData* _dirData = _dir->data;
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
    uint64_t current_time_nanoseconds = 0;
    xtGetTime(&current_time, &current_time_nanoseconds);
    XTTime time = { 0 };
    xtGetTimeFromUnix(current_time, &time);
    uint16_t dosdate = _create_date(time.year - 1980, time.month, time.mday);
    uint16_t dostime = _create_time(time.hour, time.minutes, time.seconds);

    // 3. Резервируем кластер для нового файла
    uint32_t firstCluster = find_free_cluster(mp);
    xtDebugPrint("firstCluster %u\n", firstCluster);
    set_next_cluster(mp, firstCluster, EOC);

    uint32_t parentFirstCluster = _dirData->firstCluster;
    uint32_t parentParentCluster = _dirData->parentCluster;
    uint32_t parentIndex = _dirData->index;
    uint32_t newIndex = vfatFindFreeEntry(_dir);
    xtDebugPrint("newIndex %u\n", newIndex);
    _dirData->firstCluster = firstCluster;
    _dirData->parentCluster = parentFirstCluster;
    _dirData->index = newIndex;
    XTFileInfo info = { 0 };
    info.createdTime = current_time;
    info.lastAccessTime = current_time;
    info.lastWriteTime = current_time;
    info.FileSize = 0;
    info.PhysicalSize = 0;
    info.flags = flags;
    xtCopyString(info.name, left, 256);
    xtSetFileInfo(_dir, &info);
    _dirData->firstCluster = parentFirstCluster;
    _dirData->parentCluster = parentParentCluster;
    _dirData->index = parentIndex;

    //Create dots
    if (attr & SUBDIR) {
        sfn_dir_entry dot = {
            .name = ".         ",
            .ext = "   ",
            .attributes = SUBDIR,
            .create_date = dosdate,
            .create_time = dostime,
            .firstClusterLow = firstCluster & 0xffff,
            .firstClusterHigh = firstCluster >> 16,
            .last_modifided_date = 0,
            .last_modifided_time = 0
        };
        sfn_dir_entry dotdot = {
            .name = "..        ",
            .ext = "   ",
            .attributes = SUBDIR,
            .create_date = dosdate,
            .create_time = dostime,
            .firstClusterLow = _dirData->firstCluster & 0xffff,
            .firstClusterHigh = _dirData->firstCluster >> 16,
            .last_modifided_date = 0,
            .last_modifided_time = 0
        };
        uint32_t parentCluster = _dirData->parentCluster;
        _dirData->firstCluster = firstCluster;
        _dirData->parentCluster = EOC;
        xtWriteFile(_dir, &dot, 0 * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        xtWriteFile(_dir, &dotdot, 1 * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        _dirData->firstCluster = firstCluster;
        _dirData->parentCluster = parentCluster;
        // В vfatCreateFile перед записью точек:
        uint8_t zero_buffer[SECTOR_SIZE] = {0};
        uint32_t cluster_size = fsData->sectors_per_cluster * fsData->bytes_per_sector;
        for (uint32_t i = 0; i < fsData->sectors_per_cluster; i++) {
            uint64_t phys_offset = fsData->data_offset + (uint64_t)(firstCluster - 2) * cluster_size + (i * SECTOR_SIZE);
            xtWriteFile(mp->device, zero_buffer, phys_offset, SECTOR_SIZE, NULL);
        }
    }

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
    data->firstCluster = sfn.firstClusterLow | ((uint32_t)sfn.firstClusterHigh << 16);
    // data->fileSize = sfn.file_size; // Если бы поле было, но ты используешь xtGetFileInfo
    // data->index = 0; // Сбрасываем для чтения контента файла
    data->parentCluster = parentCluster;
    *out = parentDir;
    return XT_SUCCESS;
}

XTResult vfatDeleteFile(XTMountPoint* mp, const char* path) {
    char* left = NULL;
    XTFile* parentDir = NULL;
    vfatFindFile(mp, path, &left, &parentDir);
    vfatFileData* data = parentDir->data;
    uint32_t parentCluster = data->firstCluster;
    sfn_dir_entry sfn;
    // Читаем данные файла по найденному индексу
    xtReadFile(parentDir, &sfn, data->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
    sfn.name[0] = 0xe5;
    xtWriteFile(parentDir, &sfn, data->index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
    uint32_t firstCluster = sfn.firstClusterLow | (uint32_t)sfn.firstClusterHigh << 16;
    sfn_dir_entry temp_sfn = { 0 };
    int lfnCount = 0;
    uint8_t order = 0;
    do {
        xtReadFile(parentDir, &temp_sfn, (data->index - lfnCount - 1) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        order = temp_sfn.name[0];
        if (temp_sfn.attributes == 0xf) {
            temp_sfn.name[0] = 0xe5;
            xtWriteFile(parentDir, &temp_sfn, (data->index - lfnCount - 1) * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
            ++lfnCount;
        }
    } while(temp_sfn.attributes == 0xf && !(order & ORed));
    do {
        firstCluster = get_next_cluster(mp, firstCluster);
        set_next_cluster(mp, firstCluster, 0);
    } while (firstCluster != EOC);
    xtHeapFree(data);
    xtHeapFree(parentDir);
    return XT_SUCCESS;
}

XTResult vfatOpenDirectory(XTMountPoint* mp, const char* path, XTDirectory* out) {
    char* left = NULL;
    XTFile* dir = NULL;
    vfatFindFile(mp, path, &left, &dir);
    out->data = dir;
    return XT_SUCCESS;
}


XTResult vfatReadDirectory(XTDirectory* dir, XTFileInfo* info) {
    XTFile* _fdir = dir->data;
    uint64_t index = 0;
    lfn_dir_entry lfnBuffer[20];
    int lfnCount = 0;
    for (uint64_t i = 0; i < dir->pos; ++i) {
        sfn_dir_entry sfn = { 0 };
        do {
            xtReadFile(_fdir, &sfn, index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
            ++index;
        } while(sfn.attributes == 0xf);
    }
    sfn_dir_entry sfn = { 0 };
    do {
        xtReadFile(_fdir, &sfn, index * sizeof(sfn_dir_entry), sizeof(sfn_dir_entry), NULL);
        ++index;
        if (sfn.attributes == 0xf) {
            xtCopyMem(&lfnBuffer[lfnCount++], &sfn, sizeof(lfn_dir_entry));
        }
    } while(sfn.attributes == 0xf);
    if (sfn.name[0] == 0) return XT_END_OF_FILE;
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
    xtUTF16toUTF8(utf16Name, info->name);
    vfatFSData* fsData = _fdir->mountPoint->data;
    info->FileSize = sfn.file_size;
    info->PhysicalSize = 
        (sfn.file_size / (fsData->bytes_per_sector * fsData->sectors_per_cluster) + 1) * 
        (fsData->bytes_per_sector * fsData->sectors_per_cluster);
    vfatSFNtoFileInfo(&sfn, info);
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
    xtDebugPrint("fsData->data_offset %x\n", fsData->data_offset);
    xtHeapAlloc(sizeof(XTFile), &fsData->rootDir);
    fsData->rootDir->IO = &vfatFileIO;
    fsData->rootDir->mountPoint = mp;
    xtHeapAlloc(sizeof(vfatFileData), &fsData->rootDir->data);

    vfatFileData* _vfatFileData = fsData->rootDir->data;
    _vfatFileData->firstCluster = ebpb.root_cluster;
    _vfatFileData->index = 0;
    _vfatFileData->parentCluster = EOC;
    mp->data = fsData;
    return XT_SUCCESS;
}

XTFileSystemIO vfatIO = {
    .MakeFS = vfatMakeFS,
    .Mount = vfatMount,
    .CreateFile = vfatCreateFile,
    .OpenFile = vfatOpenFile,
    .OpenDirectory = vfatOpenDirectory,
    .ReadDirectory = vfatReadDirectory
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
