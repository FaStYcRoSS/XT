#ifndef __XT_ACPI_H__
#define __XT_ACPI_H__

#include <xt/result.h>

#include <stdint.h>

typedef struct DescriptionHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    uint64_t OEMTID;
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed)) DescriptionHeader;

typedef struct XSDT {
    DescriptionHeader Header;
    uint64_t          Tables[1];
} __attribute__((packed)) XSDT;

typedef struct RSDP {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    XSDT* XsdtAddress;
    uint8_t ExtendedCheckSum;
    uint8_t Reserved[3];
} __attribute__((packed)) RSDP;

typedef struct MMECSPAAS {
    uint64_t BaseAddress;
    uint16_t PCISegmentGroupNumber;
    uint8_t StartBusNumber;
    uint8_t EndBusNumber;
    uint32_t Reserved;
} __attribute__((packed)) MMECSPAAS;

typedef struct MCFG {
    DescriptionHeader Header;
    uint64_t Reserved;
    MMECSPAAS ConfigurationSpacesFirst;
} __attribute__((packed)) MCFG;

XTResult xtACPIInit();

XTResult xtFindACPITable(const char* signature, void** out);

#endif