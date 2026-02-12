#include <xt/io.h>
#include <xt/kernel.h>

typedef struct Tar {
    char filename[100]; //100
    char mode[8]; //108
    char uid[8]; //116
    char gid[8]; //124
    char size[12]; //136
    char mtime[12]; //148
    char chksum[8]; //156
    char typeflag[1]; //157
    char link[100]; //257
    char magic[6]; //263
    char version[2]; //265
    char userOwner[32]; //297
    char groupOwner[32]; //329
    char devMajor[8]; //337
    char devMinor[8]; //345
    char prefix[167]; //512
} Tar;

int oct2bin(unsigned char *str, int size) {
    int n = 0;
    unsigned char *c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}
XTResult xtStringCmp(const char* left, const char* right, uint64_t max);

void* userProgram = NULL;

XTResult xtRamDiskInit() {

    Tar* header = gKernelBootInfo->initrd;
    userProgram = (char*)header + 0x400;

    return XT_SUCCESS;

}